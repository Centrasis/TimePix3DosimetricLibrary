#include "RadiationAngleReconstructor.h"
#include "MultiplattformTypes.h"
#include <opencv2/core.hpp>
#include "OpenCLExecutor.h"
#include <thread>
#include <math.h>
#include <limits>
#ifdef USE_DEBUG_RENDERS
  #include "OCLDebugHelpers.h"
#endif
#ifdef WIN32
  #include <Windows.h>
#endif

#ifdef _DO_ALOGRITHM_STATISTICS_
  #include "Tpx3AlgorithmStatistics.h"
#endif

#include "PostprocessingThread.h"

//#define MEASURE_DELTATIME

std::default_random_engine initPseudoRandom()
{
	std::default_random_engine retVal;
	return retVal;
}

MUTEXTYPE CREATEMUTEX(RadiationAngleReconstructor::staticInnerLock);
MUTEXTYPE CREATEMUTEX(RadiationAngleReconstructor::staticOuterLock);

void InitOCLKernel(FOCLKernel& kernel)
{
	if (!OpenCLExecutor::getExecutor().InitKernel(kernel))
		throw "Can't init shadow angle reconstructor kernels!";
}

FOCLKernel RadiationAngleReconstructor::BaseFilterSetupRatioCalcKernel;
FOCLKernel RadiationAngleReconstructor::BaseOverallShadowRatioCalcKernel;
FOCLKernel RadiationAngleReconstructor::BaseCalcShadowsFromFilterSetup;

float RadiationAngleReconstructor::AngleRange = 60.f * (M_PI / 180.f);

float RadiationAngleReconstructor::ShadowThreshold = 1.1f;//1.05f;
float RadiationAngleReconstructor::MaxPercentageOverThreshold = 0.1f;

char RadiationAngleReconstructor::RepeatsCount = 1;

FVector3D RadiationAngleReconstructor::radiationAngleResult;
FShadowSetup RadiationAngleReconstructor::resultShadowSetup;
size_t RadiationAngleReconstructor::MaxThreadCount = 48;
size_t RadiationAngleReconstructor::TestIterations = 20;
unsigned int RadiationAngleReconstructor::precision = 3;
bool RadiationAngleReconstructor::firstConstructorCall = true;
std::vector<RadiationAngleReconstructor*> RadiationAngleReconstructor::waitingCalcs;
float RadiationAngleReconstructor::previousFoundRotation[2] = {0.f, 0.f};

std::random_device RadiationAngleReconstructor::realRandom;
std::default_random_engine RadiationAngleReconstructor::pseudoRandom = initPseudoRandom();

FFilterSetup RadiationAngleReconstructor::FilterSetup = FFilterSetup();

float RadiationAngleReconstructor::currentScore = 0.f;

RadiationAngleReconstructor::RadiationAngleReconstructor()
{
	integrationResult = NULL;
	if (firstConstructorCall)
	{
		firstConstructorCall = false;
		RadiationAngleReconstructor::BaseFilterSetupRatioCalcKernel = loadOCLKernelAndConstants(ShadowAngleFinder, "calcRatioOfFilterSetup", { (std::pair<std::string, std::string>("VERTEX_COUNT", std::to_string(FilterSetup.Filters[0].geometry.size()))) });
		RadiationAngleReconstructor::BaseOverallShadowRatioCalcKernel = loadOCLKernelAndConstants(ShadowAngleFinder, "calcOverallShadowRatio", { (std::pair<std::string, std::string>("VERTEX_COUNT", std::to_string(FilterSetup.Filters[0].geometry.size()))) });
		RadiationAngleReconstructor::BaseCalcShadowsFromFilterSetup = loadOCLKernelAndConstants(shadowProjection, "calcShadows", { (std::pair<std::string, std::string>("VERTEX_COUNT", std::to_string(FilterSetup.Filters[0].geometry.size()))) });

		InitOCLKernel(BaseFilterSetupRatioCalcKernel);
		InitOCLKernel(BaseOverallShadowRatioCalcKernel);
		InitOCLKernel(BaseCalcShadowsFromFilterSetup);

		std::thread CalculationResultSelector(ExecSelectCalculationResult);
		CalculationResultSelector.detach();
	}
}

//Box-Mueller Trafo; normed til radius of 2
FVector2D getGaussianCoord(FVector2D normedRandoms)
{
	FVector2D retVal = FVector2D(sqrt((-2) * log(normedRandoms.X)) * cos(2 * M_PI*normedRandoms.Y), sqrt((-2) * log(normedRandoms.X)) * sin(2 * M_PI*normedRandoms.Y));
	//[norm by 2]
	retVal.X /= 2;
	retVal.Y /= 2;

	return retVal;
}

RadiationAngleReconstructor::RadiationAngleReconstructor(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> FrameBuffer)
{
	integrationResult = FrameBuffer;

	if (firstConstructorCall)
	{
		firstConstructorCall = false;
		RadiationAngleReconstructor::BaseFilterSetupRatioCalcKernel = loadOCLKernelAndConstants(ShadowAngleFinder, "calcRatioOfFilterSetup", { (std::pair<std::string, std::string>("VERTEX_COUNT", std::to_string(FilterSetup.Filters[0].geometry.size()))) });
		RadiationAngleReconstructor::BaseOverallShadowRatioCalcKernel = loadOCLKernelAndConstants(ShadowAngleFinder, "calcOverallShadowRatio", { (std::pair<std::string, std::string>("VERTEX_COUNT", std::to_string(FilterSetup.Filters[0].geometry.size()))) });
		RadiationAngleReconstructor::BaseCalcShadowsFromFilterSetup = loadOCLKernelAndConstants(shadowProjection, "calcShadows", { (std::pair<std::string, std::string>("VERTEX_COUNT", std::to_string(FilterSetup.Filters[0].geometry.size()))) });

		InitOCLKernel(BaseFilterSetupRatioCalcKernel);
		InitOCLKernel(BaseOverallShadowRatioCalcKernel);
		InitOCLKernel(BaseCalcShadowsFromFilterSetup);

		std::thread CalculationResultSelector(ExecSelectCalculationResult);
		CalculationResultSelector.detach();
	}
}

RadiationAngleReconstructor::~RadiationAngleReconstructor()
{
	integrationResult = NULL;
}

void RadiationAngleReconstructor::ExecCalculation()
{
#if defined(WIN32) && defined(MEASURE_DELTATIME)
	unsigned long long time1 = GetTickCount64();
#endif
#ifdef _DO_ALOGRITHM_STATISTICS_
	size_t algorithmIndex = Tpx3AlgorithmStatistics::RadiationAngleAlgorithmStatistics.GetLatestActiveAlgorithmInstance();
	if(algorithmIndex == NO_ALGORITHM_IDX)
		algorithmIndex = Tpx3AlgorithmStatistics::RadiationAngleAlgorithmStatistics.OpenNewAlgorithmInstance();
#endif
	FOCLKernel overallShadowRatioCalcKernel = FOCLKernel(RadiationAngleReconstructor::BaseOverallShadowRatioCalcKernel);

	std::shared_ptr<FFilterSetup::CLFilterGeometry> filterGeometry = NULL;
	OCLTypedVariable<cl_uint> shadowPixelCount = OCLTypedVariable<cl_uint>((cl_uint)0);
	OCLTypedVariable<float> overallShadowRatio = OCLTypedVariable<float>(0.0f);
	if (*integrationResult == NULL)
		return;
	cl::Image* img = (cl::Image*)integrationResult->getCLMemoryObject(NULL);

	size_t width = img->getImageInfo<CL_IMAGE_WIDTH>();
	size_t height = img->getImageInfo<CL_IMAGE_HEIGHT>();

	float AngleWeight = 0.f;

	struct FShadowParticle
	{
		float angles[2];
		float score;

		FShadowParticle(float angleX, float angleY, float score)
		{
			angles[0] = angleX;
			angles[1] = angleY;
			this->score = score;
		}
	};
	MUTEXTYPE threadLock;
	CREATEMUTEX(threadLock);

	std::vector<FShadowParticle> globalParticles;
	for (size_t i = 0; i < RadiationAngleReconstructor::MaxThreadCount; i++)
		globalParticles.push_back(FShadowParticle(RadiationAngleReconstructor::previousFoundRotation[0], RadiationAngleReconstructor::previousFoundRotation[1], RadiationAngleReconstructor::currentScore * 0.9f));

	
	filterGeometry = FilterSetup.generateCLFilterGeometry();
	OCLTypedVariable<cl_uint, ASPrivate> shadowCount(FilterSetup.Filters.size());

	overallShadowRatioCalcKernel.Arguments.push_back(*integrationResult);
	overallShadowRatioCalcKernel.Arguments.push_back(&shadowPixelCount);
	overallShadowRatioCalcKernel.globalThreadCount = cl::NDRange(width, height);
	overallShadowRatioCalcKernel.localThreadCount = OpenCLExecutor::getExecutor().getMaxLocalNDRange(overallShadowRatioCalcKernel.globalThreadCount);//cl::NDRange(16, 16);

	//calc overall shadow Ratio
	OpenCLExecutor::getExecutor().RunKernel(overallShadowRatioCalcKernel);
	OpenCLExecutor::getExecutor().GetResultOf(overallShadowRatioCalcKernel, shadowPixelCount);
	overallShadowRatio[0] = ((float)shadowPixelCount[0]) / ((float)width * height);
	overallShadowRatio.setVariableChanged(true);
	integrationResult->releaseCLMemory();

	//init current score
	float previousScore = RadiationAngleReconstructor::currentScore;
	RadiationAngleReconstructor::currentScore = 0.f;

	int maxAmountOfAngleSteps = (int)std::pow(10, RadiationAngleReconstructor::precision);

	//hard init with zero -> minimum are 2 iterations
	float previousBest = 0.f;
	// FINISHED INITIALIZATION
	for (size_t j = 0; j < RadiationAngleReconstructor::TestIterations; j++)
	{
		// BEGIN THREADED PART
		std::vector<std::thread> workers;
		//std::thread worker;
		for (size_t i = 0; i < RadiationAngleReconstructor::MaxThreadCount; i++)
		{
			workers.push_back(std::thread([&]()
			{
				ACQUIRE_MUTEX(threadLock);
				float localScore = globalParticles[0].score;
				OCLTypedVariable<float, ASPrivate, 2> radiationAngle = OCLTypedVariable<float, ASPrivate, 2>(new float[2]{ globalParticles[0].angles[0], globalParticles[0].angles[1] }, "", true, ATRead);
				globalParticles.erase(globalParticles.begin());
				RELEASE_MUTEX(threadLock);
				// THREAD INITIALIZATION
				float shadowDirVariation = 1.f;

				OCLTypedVariable<cl_uint> shadowedPixelInSetupCount = OCLTypedVariable<cl_uint>((cl_uint)0);
				OCLTypedVariable<cl_uint> pixelInSetupCount = OCLTypedVariable<cl_uint>((cl_uint)0);
				OCLTypedVariable<cl_uint> theoreticalArea = OCLTypedVariable<cl_uint>((cl_uint)0);
				OCLTypedVariable<cl_uint> passedItems = OCLTypedVariable<cl_uint>((cl_uint)0);
				float shadowSetupRatio = 0.f;

				OCLDynamicTypedBuffer<FShadow> localShadowArray = OCLDynamicTypedBuffer<FShadow>();
				localShadowArray.resizeBuffer(FilterSetup.Filters.size());

				//init kernels with pin size
				FOCLKernel filterSetupRatioCalcKernel = FOCLKernel(RadiationAngleReconstructor::BaseFilterSetupRatioCalcKernel);
				filterSetupRatioCalcKernel.globalThreadCount = cl::NDRange(width, height);
				filterSetupRatioCalcKernel.localThreadCount = OpenCLExecutor::getExecutor().getMaxLocalNDRange(filterSetupRatioCalcKernel.globalThreadCount);
				filterSetupRatioCalcKernel.Arguments.push_back(*integrationResult);
				filterSetupRatioCalcKernel.Arguments.push_back(&localShadowArray);
				filterSetupRatioCalcKernel.Arguments.push_back(&shadowCount);
				filterSetupRatioCalcKernel.Arguments.push_back(&pixelInSetupCount);
				filterSetupRatioCalcKernel.Arguments.push_back(&shadowedPixelInSetupCount);
				filterSetupRatioCalcKernel.Arguments.push_back(&overallShadowRatio);
				filterSetupRatioCalcKernel.Arguments.push_back(&theoreticalArea);
				filterSetupRatioCalcKernel.Arguments.push_back(&passedItems);


				FOCLKernel calcShadowsFromFilterSetup = FOCLKernel(RadiationAngleReconstructor::BaseCalcShadowsFromFilterSetup);
				calcShadowsFromFilterSetup.Arguments.push_back(&filterGeometry->verticies);
				calcShadowsFromFilterSetup.Arguments.push_back(&filterGeometry->detector);
				calcShadowsFromFilterSetup.Arguments.push_back(&localShadowArray);
				calcShadowsFromFilterSetup.Arguments.push_back(&radiationAngle);
				calcShadowsFromFilterSetup.globalThreadCount = cl::NDRange(FilterSetup.Filters.size(), FilterSetup.Filters[0].geometry.size());
				calcShadowsFromFilterSetup.localThreadCount = cl::NDRange(1, FilterSetup.Filters[0].geometry.size());

				OpenCLExecutor::getExecutor().createWorkgroup(calcShadowsFromFilterSetup);
				//OpenCLExecutor::getExecutor().appendKernelToQueueOf(calcShadowsFromFilterSetup, filterSetupRatioCalcKernel);

				// THREAD CALCULATION

				shadowedPixelInSetupCount[0] = 0;
				shadowedPixelInSetupCount.setVariableChanged(true);
				pixelInSetupCount[0] = 0;
				pixelInSetupCount.setVariableChanged(true);

				if (localScore > RadiationAngleReconstructor::ShadowThreshold * overallShadowRatio[0])
				{
					shadowDirVariation = 1.f - localScore;// ((localScore - RadiationAngleReconstructor::ShadowThreshold * overallShadowRatio[0]) / (1.f - RadiationAngleReconstructor::ShadowThreshold * overallShadowRatio[0]));
					if (shadowDirVariation > 1.f)
						shadowDirVariation = 1.f;
					else
						if (shadowDirVariation < 0.02f)
							shadowDirVariation = 0.02f;
				}

				int angleY = (int)(radiationAngle[1] * maxAmountOfAngleSteps);
				int angleX = (int)(radiationAngle[0] * maxAmountOfAngleSteps);

				angleX += RadiationAngleReconstructor::getNextRandom((unsigned int)(M_PI*shadowDirVariation*maxAmountOfAngleSteps));
				angleY += RadiationAngleReconstructor::getNextRandom((unsigned int)(M_PI*shadowDirVariation*maxAmountOfAngleSteps));
				angleX -= (int)(M_PI_2 * shadowDirVariation * maxAmountOfAngleSteps);
				angleY -= (int)(M_PI_2 * shadowDirVariation * maxAmountOfAngleSteps);

				//keep angles [-RadiationAngleReconstructor::AngleRange..RadiationAngleReconstructor::AngleRange]
				int MaxAngleX = RadiationAngleReconstructor::AngleRange * maxAmountOfAngleSteps;
				int MaxAngleY = M_PI_2 * maxAmountOfAngleSteps;
				if (angleX > 0.f)
				{
					if (angleX / MaxAngleX > 1)
						angleX -= round(angleX / MaxAngleX) * MaxAngleX;
					if (angleX > MaxAngleX)
						angleX = angleX - MaxAngleX;
				}
				else
				{
					if (angleX != 0.f && abs(angleX) / MaxAngleX > 1)
						angleX += round(abs(angleX) / MaxAngleX) * MaxAngleX;
					if(angleX < -MaxAngleX)
						angleX = -1 * (abs(angleX) - MaxAngleX);
				}
				if (angleY > 0.f)
				{
					if (angleY / MaxAngleY > 1)
						angleY -= round(angleY / MaxAngleY) * MaxAngleY;
					if (angleY > MaxAngleY)
						angleY = angleY - MaxAngleY;
				}
				else
				{
					if (angleY != 0.f && abs(angleY) / MaxAngleY > 1)
						angleY += round(abs(angleY) / MaxAngleY) * MaxAngleY;
					if (angleY < -MaxAngleY)
						angleY = -1 * (abs(angleY) - MaxAngleY);
				}

				radiationAngle[1] = (float)angleY;
				radiationAngle[0] = (float)angleX;
				radiationAngle[1] /= (float)maxAmountOfAngleSteps;
				radiationAngle[0] /= (float)maxAmountOfAngleSteps;

				radiationAngle.setVariableChanged(true);
				ACQUIRE_MUTEX(threadLock);
				OpenCLExecutor::getExecutor().RunKernel(calcShadowsFromFilterSetup);
				filterGeometry->verticies.releaseCLMemory();
				filterGeometry->detector.releaseCLMemory();
				RELEASE_MUTEX(threadLock);
				ACQUIRE_MUTEX(threadLock);
				OpenCLExecutor::getExecutor().RunKernel(filterSetupRatioCalcKernel);
				integrationResult->releaseCLMemory();
				RELEASE_MUTEX(threadLock);
				OpenCLExecutor::getExecutor().GetResultOf(filterSetupRatioCalcKernel, shadowedPixelInSetupCount);
				OpenCLExecutor::getExecutor().GetResultOf(filterSetupRatioCalcKernel, pixelInSetupCount);
				shadowSetupRatio = (float)shadowedPixelInSetupCount[0] / (float)pixelInSetupCount[0];
				ACQUIRE_MUTEX(threadLock);
				float weightFromAngle = AngleWeight * (1.f - (RadiationAngleReconstructor::AngleRange - abs(radiationAngle[0]))/RadiationAngleReconstructor::AngleRange);
				shadowSetupRatio = shadowSetupRatio * (1.f + weightFromAngle);
				globalParticles.push_back(FShadowParticle(radiationAngle[0], radiationAngle[1], shadowSetupRatio));
				RELEASE_MUTEX(threadLock);
#ifdef USE_DEBUG_RENDERS
				OCLDebugHelpers::drawColoredShadows(integrationResult, &localShadowArray);
#endif
			}));
		}

		while (workers.size() > 0)
		{
			std::thread& t = workers.back();
			if (t.joinable())
				t.join();

			workers.pop_back();
		};
		std::sort(globalParticles.begin(), globalParticles.end(), [](FShadowParticle& l, FShadowParticle& r) { return l.score > r.score; });

#ifdef _DO_ALOGRITHM_STATISTICS_
		Tpx3AlgorithmStatistics::RadiationAngleAlgorithmStatistics[algorithmIndex].AddCicle();
#endif

		//Select new particles for next iteration from gaussian distribution
		std::vector<FShadowParticle> newParticles;
		float MeanOfGauss = overallShadowRatio[0] * RadiationAngleReconstructor::ShadowThreshold;
		while (newParticles.size() < RadiationAngleReconstructor::MaxThreadCount)
		{
			float gauss = 1.2f;
			//allow for 20% overshoot assuming that it is more likely that a score is near 1.f than near 0.f
			while (gauss < 0.f || gauss > 1.1f)
			{
				float rand1 = getNextRandom(10000) / 10000.f;
				float rand2 = getNextRandom(10000) / 10000.f;
				//1D-Box-Mueller trafo
				gauss = (std::sqrt(-2.f * std::log(rand1)) * std::cos(2.f*M_PI*rand2)) * 0.1f + MeanOfGauss;
			}
			for (auto p : globalParticles)
				if (p.score <= gauss)
				{
					newParticles.push_back(p);
					break;
				}
		}
		globalParticles.erase(globalParticles.begin() + RadiationAngleReconstructor::MaxThreadCount, globalParticles.end());
		if (globalParticles[0].score >= MeanOfGauss)
		{
			if (globalParticles[0].score <= previousBest * 1.0005f)
			{
				break;
			}
			else
			{
				float best = 0.f;
				for (size_t i = 0; i < RadiationAngleReconstructor::MaxThreadCount; i++)
				{
					globalParticles[i] = newParticles[i];
					if (best < globalParticles[i].score)
						best = globalParticles[i].score;
				}
				previousBest = best;
			}
		}
		else
		{
			for (size_t i = 0; i < RadiationAngleReconstructor::MaxThreadCount; i++)
			{
				globalParticles[i] = newParticles[i];
			}
		}
		float meanScore = 0.f;
		for (size_t i = 0; i < RadiationAngleReconstructor::MaxThreadCount * 0.4; i++)
			meanScore += globalParticles[i].score;
		meanScore /= (size_t) RadiationAngleReconstructor::MaxThreadCount * 0.4;
	}
	DESTROYMUTEX(threadLock);

	// FINISHED CALCULATION
	// BEGIN UNTHREADED CLEANUP

	ACQUIRE_MUTEX(staticInnerLock);
#if defined(WIN32) && defined(MEASURE_DELTATIME)
	unsigned long long time2 = GetTickCount64();
	std::printf("DeltaTime: %u ms\n", time2 - time1);
#endif
	std::sort(globalParticles.begin(), globalParticles.end(), [](FShadowParticle& l, FShadowParticle& r) { return l.score > r.score; });

	float probablySum = 0.f;
	size_t maxCount = 0;
	for (size_t i = 0; i < globalParticles.size(); i++)
		if (globalParticles[i].score >= overallShadowRatio[0] * RadiationAngleReconstructor::ShadowThreshold)
		{
			probablySum += globalParticles[i].score;
			maxCount = i;
		}
		else
			break;
	//ensure, that all scores add up to 1.f
	for (size_t i = 0; i <= maxCount; i++)
			globalParticles[i].score /= probablySum;

	if (maxCount > 0)
	{
		float angles[2] = { 0.f, 0.f };
		for (size_t i = 0; i <= maxCount; i++)
		{
			angles[0] += globalParticles[i].score * globalParticles[i].angles[0];
			angles[1] += globalParticles[i].score * globalParticles[i].angles[1];
		}

		// RECALCULATE SHADOW SETUP FROM VALUES OF THE BEST GLOBAL PARTICLE
		OCLDynamicTypedBuffer<FShadow> ShadowArray = OCLDynamicTypedBuffer<FShadow>();
		ShadowArray.resizeBuffer(FilterSetup.Filters.size());
		OCLTypedVariable<float, ASPrivate, 2> radiationAngle = OCLTypedVariable<float, ASPrivate, 2>((float*)NULL, "", true, ATRead);
		radiationAngle[0] = angles[0];
		radiationAngle[1] = angles[1];

		FOCLKernel calcShadowsFromFilterSetup = FOCLKernel(RadiationAngleReconstructor::BaseCalcShadowsFromFilterSetup);
		calcShadowsFromFilterSetup.Arguments.push_back(&filterGeometry->verticies);
		calcShadowsFromFilterSetup.Arguments.push_back(&filterGeometry->detector);
		calcShadowsFromFilterSetup.Arguments.push_back(&ShadowArray);
		calcShadowsFromFilterSetup.Arguments.push_back(&radiationAngle);
		calcShadowsFromFilterSetup.globalThreadCount = cl::NDRange(FilterSetup.Filters.size(), FilterSetup.Filters[0].geometry.size());
		calcShadowsFromFilterSetup.localThreadCount = cl::NDRange(1, FilterSetup.Filters[0].geometry.size());

		OCLTypedVariable<cl_uint> shadowedPixelInSetupCount = OCLTypedVariable<cl_uint>((cl_uint)0);
		OCLTypedVariable<cl_uint> pixelInSetupCount = OCLTypedVariable<cl_uint>((cl_uint)0);
		OCLTypedVariable<cl_uint> theoreticalArea = OCLTypedVariable<cl_uint>((cl_uint)0);
		OCLTypedVariable<cl_uint> passedItems = OCLTypedVariable<cl_uint>((cl_uint)0);

		//init kernels with pin size
		FOCLKernel filterSetupRatioCalcKernel = FOCLKernel(RadiationAngleReconstructor::BaseFilterSetupRatioCalcKernel);
		filterSetupRatioCalcKernel.globalThreadCount = cl::NDRange(width, height);
		filterSetupRatioCalcKernel.localThreadCount = OpenCLExecutor::getExecutor().getMaxLocalNDRange(filterSetupRatioCalcKernel.globalThreadCount);
		filterSetupRatioCalcKernel.Arguments.push_back(*integrationResult);
		filterSetupRatioCalcKernel.Arguments.push_back(&ShadowArray);
		filterSetupRatioCalcKernel.Arguments.push_back(&shadowCount);
		filterSetupRatioCalcKernel.Arguments.push_back(&pixelInSetupCount);
		filterSetupRatioCalcKernel.Arguments.push_back(&shadowedPixelInSetupCount);
		filterSetupRatioCalcKernel.Arguments.push_back(&overallShadowRatio);
		filterSetupRatioCalcKernel.Arguments.push_back(&theoreticalArea);
		filterSetupRatioCalcKernel.Arguments.push_back(&passedItems);

		radiationAngle.setVariableChanged(true);
		OpenCLExecutor::getExecutor().RunKernel(calcShadowsFromFilterSetup);
		OpenCLExecutor::getExecutor().GetResultOf(calcShadowsFromFilterSetup, ShadowArray);
		OpenCLExecutor::getExecutor().RunKernel(filterSetupRatioCalcKernel);
		OpenCLExecutor::getExecutor().GetResultOf(filterSetupRatioCalcKernel, shadowedPixelInSetupCount);
		OpenCLExecutor::getExecutor().GetResultOf(filterSetupRatioCalcKernel, pixelInSetupCount);
		RadiationAngleReconstructor::currentScore = (float)shadowedPixelInSetupCount[0] / (float)pixelInSetupCount[0];

		RadiationAngleReconstructor::radiationVector = FVector3D(cos(radiationAngle[0])*sin(radiationAngle[1]), sin(radiationAngle[0])*sin(radiationAngle[1]), cos(radiationAngle[1]));
		RadiationAngleReconstructor::previousFoundRotation[0] = radiationAngle[0];
		RadiationAngleReconstructor::previousFoundRotation[1] = radiationAngle[1];

		RadiationAngleReconstructor::radiationAngleResult = FVector3D(radiationAngle[0], radiationAngle[1]);
		RadiationAngleReconstructor::resultShadowSetup.rotationAngle2D = globalParticles[0].angles[1];
		RadiationAngleReconstructor::resultShadowSetup.score = globalParticles[0].score;
		RadiationAngleReconstructor::resultShadowSetup.singleShadows.clear();
		for (size_t i = 0; i < ShadowArray.getBufferLength(); i++)
			RadiationAngleReconstructor::resultShadowSetup.singleShadows.push_back(ShadowArray[i]);
	}
	else
	{
		repeatCount++;
#ifdef _DO_ALOGRITHM_STATISTICS_
		Tpx3AlgorithmStatistics::RadiationAngleAlgorithmStatistics[algorithmIndex].AddRepeat();
#endif
		if (repeatCount <= RadiationAngleReconstructor::RepeatsCount)
		{
			ACQUIRE_MUTEX(RadiationAngleReconstructor::staticOuterLock);
			waitingCalcs.push_back(NULL);
			for (size_t i = waitingCalcs.size() - 1; i > 0 ; i--)
			{
				waitingCalcs[i] = waitingCalcs[i-1];
			}
			RadiationAngleReconstructor::previousFoundRotation[0] = 0.f;
			RadiationAngleReconstructor::previousFoundRotation[1] = 0.f;
			RadiationAngleReconstructor::currentScore = 0.f;
			RELEASE_MUTEX(RadiationAngleReconstructor::staticOuterLock);
			RELEASE_MUTEX(RadiationAngleReconstructor::staticInnerLock);
			return;
		}
		else
		{
			RadiationAngleReconstructor::resultShadowSetup.singleShadows.clear();
			RadiationAngleReconstructor::resultShadowSetup.score = 0.f;
			RadiationAngleReconstructor::resultShadowSetup.rotationAngle2D = 0.f;
			RadiationAngleReconstructor::radiationAngleResult = FVector3D(INFINITY, INFINITY, INFINITY);
		}
	}

	RELEASE_MUTEX(RadiationAngleReconstructor::staticInnerLock);

#ifdef _DO_ALOGRITHM_STATISTICS_
	Tpx3AlgorithmStatistics::RadiationAngleAlgorithmStatistics[algorithmIndex].FinishInstance();
#endif
	std::thread t([](FOnAngleCalcFinished onAngleCalcFinished, std::shared_ptr<OCLMemoryVariable<cl::Image2D>> integrationResult)
	{
		FShadowSetup ret = RadiationAngleReconstructor::getShadowSetup();
		onAngleCalcFinished(integrationResult, std::ref(RadiationAngleReconstructor::radiationAngleResult), std::ref(ret));
	}, onAngleCalcFinished, integrationResult);
	t.detach();

	//Memory cleanup
	delete this;
}

void RadiationAngleReconstructor::ExecSelectCalculationResult()
{
	while (true)
	{
		if (waitingCalcs.size() > 0)
		{
			waitingCalcs[0]->ExecCalculation();
			ACQUIRE_MUTEX(RadiationAngleReconstructor::staticOuterLock);
			if(waitingCalcs.size() > 0)
				waitingCalcs.erase(waitingCalcs.begin(), waitingCalcs.begin() + 1);
			RELEASE_MUTEX(RadiationAngleReconstructor::staticOuterLock);
		}
		else
		{
			Sleep(1);
		}
	}
}

void RadiationAngleReconstructor::SetFilterSetup(FFilterSetup filters)
{
	RadiationAngleReconstructor::FilterSetup = filters;
}

bool RadiationAngleReconstructor::LoadSetupFromArchive(std::string path)
{
	FILETYPE_IN file = OPEN_FILE_R(path);
	if (NOT_EOF(file))
	{
		std::string line = READ_LINE(file);

		CLOSE_FILE(file);
		return true;
	}

	CLOSE_FILE(file);
	return false;
}

void RadiationAngleReconstructor::calcParallelInRayAngle(FOnAngleCalcFinished onAngleFinished)
{
	this->onAngleCalcFinished = onAngleFinished;
	ACQUIRE_MUTEX(RadiationAngleReconstructor::staticOuterLock);
	RadiationAngleReconstructor::waitingCalcs.push_back(this);
	RELEASE_MUTEX(RadiationAngleReconstructor::staticOuterLock);
}

FShadowSetup RadiationAngleReconstructor::getShadowSetup()
{
	ACQUIRE_MUTEX(RadiationAngleReconstructor::staticInnerLock);
	FShadowSetup retSetUp = RadiationAngleReconstructor::resultShadowSetup;
	RELEASE_MUTEX(RadiationAngleReconstructor::staticInnerLock);
	return retSetUp;
}

void RadiationAngleReconstructor::AbortCalc()
{
	ACQUIRE_MUTEX(RadiationAngleReconstructor::staticOuterLock);
	waitingCalcs.clear();
	RELEASE_MUTEX(RadiationAngleReconstructor::staticOuterLock);
}

bool RadiationAngleReconstructor::IsOperationPending()
{
	ACQUIRE_MUTEX(RadiationAngleReconstructor::staticOuterLock);
	bool retVal = waitingCalcs.size() > 0;
	RELEASE_MUTEX(RadiationAngleReconstructor::staticOuterLock);
	return retVal;
}

unsigned int RadiationAngleReconstructor::getNextRandom(unsigned int bound)
{
	if (bound == 0)
		return 0;

	if (realRandom.entropy() != 0.0)
	{
		return realRandom() % bound;
	}
	else
	{
		return pseudoRandom() % bound;
	}
}

void RadiationAngleReconstructor::calcRadiationAngle(FOnAngleCalcFinished onAngleFinished, std::vector<FShadow> shadows)
{
	if (shadows.size() == 0)
		return;
}

