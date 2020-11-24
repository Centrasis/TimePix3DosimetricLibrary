#include "Tpx3DosageMeasurement.h"
#include <thread>
#include <KatherineTimepix3.h>
#ifndef __SIMULATION__
#include "katherine/KatherineConfigParser.h"
#endif
#include <string>
#include <unistd.h>
#include <MultiplattformTypes.h>
#include <sstream>
#include "PixelSorter.h"
#include <functional>
//need to include atomic first
#include <atomic>
#include <mutex>
#include "Clusterings.h"
#include "PixelClusterer.h"
#include "PostprocessingThread.h"
#ifdef __USE_OPENCL__
  #include "TimepixDataCLProc.h"
#endif
#include "PixelIntegrator.h"
#include <math.h>
#include "RadiationAngleReconstructor.h"
#ifdef USE_DEBUG_RENDERS
   #include "OCLDebugHelpers.h"
#endif

#ifndef WIN32
#include <time.h>
#endif

uint64_t ms_to_ns(uint32_t ms)
{
	return ms * 1000000;
}

void frame_started(int frame_idx)
{
	//std::printf("Start Frame\n");
}

void frame_ended(int frame_idx, bool completed, const katherine_frame_info_t *info)
{
	//std::printf("End Frame\n");
}

void Tpx3DosageMeasurement::OnPixels_received(const void *pixels, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		addPixel((katherine_px_f_toa_tot_t*)pixels + i);		
	}
}

#ifndef __SIMULATION__

Tpx3DosageMeasurement::Tpx3DosageMeasurement()
{
	currentFrameBuffer = (char*)malloc(255 * 256);
	CREATEMUTEX(lock);

	controller = this;
	currentFrameBuffer = (char*)malloc(255 * 256);
	DataBuffer = (katherine_px_f_toa_tot_t*)malloc(DataBufferSize * sizeof(katherine_px_f_toa_tot_t));

	pixelListRoot = new ListedPixel();
	pixelListRoot->pixel = new katherine_px_f_toa_tot_t();
	pixelListRoot->pixel->toa = -1.f;

	postProcessingThread = new PostprocessingThread(0, DataBuffer);
}

#else

Tpx3DosageMeasurement::Tpx3DosageMeasurement(std::string AFile)
{
	std::string simuDataPath = "/SampleData.t3pa";
	TimePixFile = OPEN_FILE_R((AFile + simuDataPath).c_str());
	basePath = AFile;
	usedTimepix = NULL;
	CREATEMUTEX(lock);
	currentFrameBuffer = cv::Mat(256, 256, CV_8UC1, cv::Scalar(0));
#ifdef ENABLE_CLUSTERING
	blurredFrame = cv::Mat(256, 256, CV_8UC1, cv::Scalar(0));
#endif
	shadowFrame = cv::Mat(256, 256, CV_8UC4, cv::Scalar(0,0,0,0));
#ifdef USE_DEBUG_RENDERS
	OCLDebugHelpers::setDefaultOutput(shadowFrame);
#endif
	CL_PixelBuffer = new OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>(NULL, 0, "CLPixelBuffer", true, ATRead);
#ifdef __USE_OPENCL__
	assert(((OpenCLExecutor*)(&TimepixDataCLProc::getExecutor()))->IsInitialized());
#endif
	postProcessingThread = new PostprocessingThread(0, CL_PixelBuffer);
}

void Tpx3DosageMeasurement::SetSimulationFileName(std::string name)
{
	if(TimePixFile != NULL)
		CLOSE_FILE(TimePixFile);

	TimePixFile = OPEN_FILE_R(name.c_str());

	//delete CL_PixelBuffer;

	//reinit buffer instead of deleteing it
	CL_PixelBuffer->releaseCLMemory();
	CL_PixelBuffer->setCurrentReadPos(0);
	CL_PixelBuffer->setCurrentWritePos(0);
	CL_PixelBuffer->setReadEndPosForCLDevice(0);
	//CL_PixelBuffer = new OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>(NULL, 0, "CLPixelBuffer", true, ATRead);
}

#endif

Tpx3DosageMeasurement::~Tpx3DosageMeasurement()
{
	//free(DataBuffer);
	if (usedTimepix)
		delete usedTimepix;

#ifdef __SIMULATION__
	CLOSE_FILE(TimePixFile);
#endif
	DESTROYMUTEX(lock);
}

void Tpx3DosageMeasurement::setFilterSetup(std::string & path, unsigned int px_width, unsigned int px_height)
{
	TpxFilters.Detector.heightIn_px = px_height;
	TpxFilters.Detector.widthIn_px = px_width;

	FILETYPE_IN AFile = OPEN_FILE_R((path).c_str());
	enum EMode { mPin, mDetector };
	//overall block mode 
	EMode mode;
	while (NOT_EOF(AFile))
	{
		std::stringstream ss(READ_LINE(AFile));
		//mode of the line (object, face, normal, name, ...)
		std::string modeLn;
		ss >> modeLn;

		if (modeLn == "o")
		{
			std::string name;
			ss >> name;

			if (name.find("Pin") != std::string::npos)
			{
				mode = mPin;
				TpxFilters.Filters.push_back(FFilter());
				TpxFilters.Filters.back().name = name;
				continue;
			}

			if (name.find("Detector") != std::string::npos)
			{
				mode = mDetector;
				continue;
			}
			throw "Wrong FilterSetup Format!\n";
			exit(-1);
		}

		if (modeLn == "v")
		{
			if (mode == mPin)
			{
				TpxFilters.Filters.back().geometry.push_back(FVector3D());
				FVector3D vec = TpxFilters.Filters.back().geometry.back();
				ss >> std::to_string(vec.X);
				ss >> std::to_string(vec.Z);
				ss >> std::to_string(vec.Y);
			}
			if (mode == mDetector)
			{
				float minX, maxX = 0.f;
				float minY, maxY = 0.f;
				FVector3D vec[4];

				//convert Z to be the Up-Direction
				ss >> std::to_string(vec[0].X);
				ss >> std::to_string(vec[0].Z);
				ss >> std::to_string(vec[0].Y);
				maxX = minX = vec[0].X;
				minY = maxY = vec[0].Y;

				for (int i = 1; i < 4; i++)
				{
					std::stringstream ss2(READ_LINE(AFile));
					std::string m;
					ss2 >> m;
					if (m != "v")
					{
						throw "Error to less verticies for detector found!\n";
						exit(-1);
					}
					ss2 >> std::to_string(vec[i].X);
					ss2 >> std::to_string(vec[i].Z);
					ss2 >> std::to_string(vec[i].Y);
					if (vec[i].X > maxX)
						maxX = vec[i].X;
					if (vec[i].X < minX)
						minX = vec[i].X;

					if (vec[i].Y > maxY)
						maxY = vec[i].Y;
					if (vec[i].Y < minY)
						minY = vec[i].Y;
				}
				
				TpxFilters.Detector.heightIn_mm = (unsigned int) (maxY - minY);
				TpxFilters.Detector.widthIn_mm = (unsigned int) (maxX - minX);
			}
		}
	}

	TpxFilters.Detector.calcPixelsPer_mm();
	currentFrameBuffer = cv::Mat(TpxFilters.Detector.widthIn_px, TpxFilters.Detector.heightIn_px, CV_8UC1);
	shadowFrame = cv::Mat(TpxFilters.Detector.widthIn_px, TpxFilters.Detector.heightIn_px, CV_8UC3);

	//TpxFilters.Filters.erase(TpxFilters.Filters.begin() + 1, TpxFilters.Filters.end());


	RadiationAngleReconstructor::SetFilterSetup(TpxFilters);
}

FFilterSetup Tpx3DosageMeasurement::getFilterSetup()
{
	return TpxFilters;
}

bool Tpx3DosageMeasurement::searchTimepix(int lowerBound, int upperBound)
{
#ifndef __SIMULATION__
	int itr = lowerBound;
	while (!(usedTimepix && usedTimepix->getChipID().length() > 0) && itr <= upperBound)
	{
		if (usedTimepix)
			delete usedTimepix;

		usedTimepix = new KatherineTimepix3(std::string("192.168.1." + std::to_string(itr)).c_str());
		if (usedTimepix->initialize() && usedTimepix->getChipID().length() > 0)
			return true;
		itr++;
	};
#else
	return true;
#endif

	return false;
}

bool Tpx3DosageMeasurement::useTimePix(KatherineTimepix3 * timepix)
{
	if (!timepix)
		return false;

	usedTimepix = timepix;
	return ((timepix->initialize()) && (usedTimepix->getChipID().length() > 0));
}

bool Tpx3DosageMeasurement::isOnlineMeasurementActive()
{
#ifdef __SIMULATION__
	return false;
#else
	return (usedTimepix);
#endif
}

bool Tpx3DosageMeasurement::areTrustedSourcesActive()
{
#ifdef __USE_COMPILETIMERESSOURCES__
	return true;
#else
	return false;
#endif
}

bool Tpx3DosageMeasurement::configureTimepix(const char * path)
{
#ifndef __SIMULATION__
	if (fileExists(path))
	{
		configParser = new KatherineConfigParser(path);
		if (!usedTimepix)
		{
			usedTimepix = new KatherineTimepix3(configParser->getIP().c_str());
			if (!usedTimepix->initialize())
			{
				delete usedTimepix;
				usedTimepix = nullptr;
				return false;
			}
		}
	}
	else
		return false;
#else
	usedTimepix = new KatherineTimepix3("0.0.0.0");
	usedTimepix->initialize();
#endif

	return true;
}

bool Tpx3DosageMeasurement::doDataDrivenMode(KatherineConfigParser * config)
{
#ifndef __SIMULATION__
	if (config != nullptr)
		configParser = config;

	bool result = (katherine_acquisition_init(&acq_info, usedTimepix->getReadoutDevice(), KATHERINE_MD_SIZE * configParser->getBufferMD_Size(), sizeof(katherine_px_f_toa_tot_t) * configParser->getBufferPixel_Size()) == 0);
#endif

	acq_info.handlers.frame_started = frame_started;
	acq_info.handlers.frame_ended = frame_ended;
	acq_info.handlers.pixels_received = std::bind<void>(&Tpx3DosageMeasurement::OnPixels_received, this, std::placeholders::_1, std::placeholders::_2);

#ifndef __SIMULATION__


	result = result && (katherine_acquisition_begin(&acq_info, configParser->getConfig(), READOUT_DATA_DRIVEN) == 0);

	//acq_info.acq_mode = ACQUISITION_MODE_EVENT_ITOT;

	if (result)
	{
		bShouldReadData = true;
		ReadDataThread = new std::thread(&Tpx3DosageMeasurement::ExecReadData, this);
	}

	return result;
#else
	SortDataThread = new PixelSorter(&BubbleSort_R<OCLTypedVariable<katherine_px_f_toa_tot_t, ASGlobal, PixelDataBufferSize>, katherine_px_f_toa_tot_t>, CL_PixelBuffer, amountOfToSortPixels, DataBufferSize, CL_PixelBuffer->getWriteBufferPtr());
	//SortDataThread = new PixelSorter(&MergeSort<PixelList, katherine_px_f_toa_tot_t>, DataBuffer, amountOfToSortPixels, DataBufferSize, &currentWriteDataPos, pixelList);

	SortDataThread->SetOnNewPixelsSorted(std::bind<void>(&Tpx3DosageMeasurement::OnNewPixelSorted, this, std::placeholders::_1, std::placeholders::_2));
	SortDataThread->StartProcessing();

	PixelIntegratorThread = new PixelIntegrator(CL_PixelBuffer, ms_to_ns(integrationTime_ms), DataBufferSize);
	PixelIntegratorThread->SetUpperLimitingThread(SortDataThread);
	PixelIntegratorThread->SetOnIntegrationFinished(std::bind(&Tpx3DosageMeasurement::OnIntegrationFinished, this
#ifdef __USE_OPENCL__
		, std::placeholders::_1
#endif
	,std::placeholders::_2)
	);
	PixelIntegratorThread->SetFrameBuffer(currentFrameBuffer);
	PixelIntegratorThread->setMinimalEventsPerFrame(minimalEventsPerFrame);
	PixelIntegratorThread->StartProcessing();
	
#ifdef ENABLE_CLUSTERING
	ClusterThread = new PixelClusterer(&PixelClustering::NeighbouringPixels, CL_PixelBuffer, MinAmountOfClusteringPixels, DataBufferSize, std::bind(&Tpx3DosageMeasurement::OnNewClustersFound, this, std::placeholders::_1));
	ClusterThread->SetUpperLimitingThread(PixelIntegratorThread);
	ClusterThread->setWorkImage(blurredFrame);
	ClusterThread->StartProcessing();
#endif
	if (postProcessingThread == nullptr)
		postProcessingThread = new PostprocessingThread(0, CL_PixelBuffer);
	postProcessingThread->SetUpperLimitingThread(SortDataThread);
	//TODO: add methods to process
	postProcessingThread->StartProcessing();

	bShouldReadData = true;
	ReadDataThread  = new std::thread(&Tpx3DosageMeasurement::ExecReadData, this);

	return true;
#endif
}

PostprocessingThread * Tpx3DosageMeasurement::getPostProcessingThread()
{
	if (postProcessingThread != nullptr)
		return postProcessingThread;
	else
	{
		postProcessingThread = new PostprocessingThread(0, CL_PixelBuffer);
		return postProcessingThread;
	}
}

void Tpx3DosageMeasurement::abortDataReadout(bool ignoreZombies)
{
	if (ReadDataThread)
	{
		bShouldReadData = false;
		acq_info.state = ACQUISITION_ABORTED;
		//ReadDataThread->join();
		ReadDataThread->detach();
#ifdef WIN32
		Sleep(200);
#else
		nanosleep((const struct timespec[]) { {0, 200000000L} }, NULL);
#endif
		delete ReadDataThread;
		RadiationAngleReconstructor::AbortCalc();
		ReadDataThread = nullptr;
		
		SortDataThread->SetUpperLimitingThread(NULL);
		PixelIntegratorThread->SetUpperLimitingThread(NULL);
		ClusterThread->SetUpperLimitingThread(NULL);
		postProcessingThread->SetUpperLimitingThread(NULL);

		if (SortDataThread->IsProcessing())
			SortDataThread->EndProcessing(ignoreZombies);
		SortDataThread = nullptr;
		if (PixelIntegratorThread->IsProcessing())
			PixelIntegratorThread->EndProcessing(ignoreZombies);
		PixelIntegratorThread = nullptr;
		if (ClusterThread && ClusterThread->IsProcessing())
			ClusterThread->EndProcessing(ignoreZombies);
		ClusterThread = nullptr;
		if (postProcessingThread && postProcessingThread->IsProcessing())
			postProcessingThread->EndProcessing(ignoreZombies);
		postProcessingThread = nullptr;
#ifndef __SIMULATION__
		katherine_acquisition_abort(&acq_info);
#endif
	}
}

void Tpx3DosageMeasurement::OnIntegrationFinished(
#ifdef __USE_OPENCL__
	std::shared_ptr<OCLMemoryVariable<cl::Image2D>> integrationResult,
#endif
	uint64_t timeStamp
)
{
#ifdef __USE_OPENCL__
	/*ACQUIRE_MUTEX(lock);
	if (!isCalcingAngle)
		isCalcingAngle = true;
	else
	{
		RELEASE_MUTEX(lock);
		return;
	}
	RELEASE_MUTEX(lock);*/
	RadiationAngleReconstructor* angleCalc = new RadiationAngleReconstructor(integrationResult);
	PostprocessingThread::registerNewIntegrationResult(integrationResult, timeStamp);
	angleCalc->calcParallelInRayAngle(std::bind(&Tpx3DosageMeasurement::OnAngleCalcFinished, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
#endif
}

#ifdef __SIMULATION__
void Tpx3DosageMeasurement::SetMaxProcessingTime_ns(uint64_t maxTime)
{
	maximalPixelTimeForReading = maxTime;
}
#endif

void Tpx3DosageMeasurement::OnAngleCalcFinished(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> image, FVector3D& angles, FShadowSetup& shadows)
{
	if (!gotFirstAngle)
		gotFirstAngle = true;
	shadowAngle = angles;
	isCalcingAngle = false;
	PostprocessingThread::registerNewShadowResult(image, angles, shadows);
}

void Tpx3DosageMeasurement::addPixel(katherine_px_f_toa_tot_t * pixel)
{
	ACQUIRE_MUTEX(lock);

	if (!(*pixel))
	{
		//std::printf("Error reading pixels! [addPixel()]\n");
		throw std::runtime_error("pixel was not valid!");
	}

	//Get real ns from raw ToA
	correctPixelToA(pixel);

#ifdef __SIMULATION__
	if (maximalPixelTimeForReading != 0)
	{
		if (pixel->toa >= maximalPixelTimeForReading)
			bShouldReadData = false;
	}
#endif
	//physically save pixel in buffer
	CL_PixelBuffer->writeNext(*pixel);

	RELEASE_MUTEX(lock);
}

void Tpx3DosageMeasurement::correctPixelToA(katherine_px_f_toa_tot_t * pixel)
{
	/*if (currentToAInterval > 50 && InOutToA < currentToAInterval - 50)	//uncertanty of 50ns
	{
		rolloverCount++;
	}

	InOutToA += rolloverCount * ToAOverflowValue;

	currentToAInterval = InOutToA;

	if (InOutToA > newestPixelToA)
		newestPixelToA = InOutToA;*/
	uint64_t ToA = pixel->toa * 25;
	float fToA = (float)pixel->ftoa * (25.f / 16.f);
	if(ToA > fToA)
		pixel->toa = ToA - (uint64_t)fToA; //One meas interval has 25ns
}

void Tpx3DosageMeasurement::OnNewPixelSorted(size_t begin, size_t end)
{
	latestPixelTime = (*CL_PixelBuffer)[end].toa;
	PostprocessingThread::registerNewDataSegment(begin, end);
}

void Tpx3DosageMeasurement::OnNewClustersFound(std::shared_ptr<ClusterList> clusters)
{
	////std::printf("new Clusters: %i\n", clusters->size());

	if (clusters->size() <= 0)
	{
		return;
	}

	PostprocessingThread::registerNewClusterList(clusters);
}

std::vector<size_t> Tpx3DosageMeasurement::getCalculationSegment()
{
	std::vector<size_t> segment;
	segment.push_back(PixelIntegratorThread->GetCurrentProcessingPos());
	if (SortDataThread->IsProcessing())
		segment.push_back(SortDataThread->GetCurrentProcessingPos() + amountOfToSortPixels);
	else
		segment.push_back(SortDataThread->GetCurrentProcessingPos());

	if (!PixelIntegratorThread->IsProcessing())
		segment.push_back(0);

	return segment;
}

cv::Mat& Tpx3DosageMeasurement::getCurrentFrame()
{
	PixelIntegratorThread->UpdateFrame(true);
	return currentFrameBuffer;
}

cv::Mat & Tpx3DosageMeasurement::getCurrentFrameWithFoundShadows()
{
	PixelIntegratorThread->UpdateFrame(true);
	FShadowSetup shadowArray = RadiationAngleReconstructor::getShadowSetup();
	
#ifndef USE_DEBUG_RENDERS
	cv::cvtColor(currentFrameBuffer, shadowFrame, cv::COLOR_GRAY2RGBA);
#else
	OCLDebugHelpers::safeExec([&]() {
#endif
		if (!shadowArray)
			return shadowFrame;

		for (size_t i = 0; i < shadowArray.singleShadows.size(); i++)
		{
			cv::ellipse(shadowFrame, cv::Point(shadowArray.singleShadows[i].middle[0], shadowArray.singleShadows[i].middle[1]), cv::Size(shadowArray.singleShadows[i].radiusX, shadowArray.singleShadows[i].radiusY), (180.f / M_PI) * shadowArray.rotationAngle2D, 0.f, 360.f, cv::Scalar(0.f, 0.f, 255), 2);
		}
#ifdef USE_DEBUG_RENDERS
	});
#endif
	return shadowFrame;
}

#ifdef ENABLE_CLUSTERING
cv::Mat & Tpx3DosageMeasurement::getCurrentFrameBlurred()
{
	ClusterThread->UpdateFrame();
	return blurredFrame;
}
#endif

void Tpx3DosageMeasurement::OnNewDataRead(katherine_acquisition_t& acq)
{
	ACQUIRE_MUTEX(lock);
#ifndef __SIMULATION__
	if (acq.state != ACQUISITION_TIMED_OUT)
	{
		//std::printf("Acquisition completed\n");
		//std::printf(" - state: %s\n", katherine_str_acquisition_status(acq.state));
		//std::printf(" - received %i complete frames\n", acq.completed_frames);
	}
	else
	{
		//std::printf("Timeout");
	}
#else
	////std::printf("new Simulation Data:\n");
#endif
	RELEASE_MUTEX(lock);
}

void Tpx3DosageMeasurement::ExecReadData()
{
	struct stringChecker
	{
		static bool isCharAlphaNumeric(char& c)
		{
			return ((int)c >= '0' && (int)c <= '9') || ((int)c >= 'A' && (int)c <= 'Z') || ((int)c >= 'a' && (int)c <= 'z') || c == '#';
		}

		static bool checkIsLineCommented(std::string& s)
		{
			int i = 0;
			while (!isCharAlphaNumeric(s[i]) && i < s.length())
				i++;

			return s[i] == '#';
		}
	};

#ifdef __SIMULATION__
	katherine_px_f_toa_tot_t pixel;
	unsigned int lineNb = 0;
	std::vector<katherine_px_f_toa_tot_t> pixels;
	pixels.clear();

#ifdef __USE_ADVACAM__
	READ_CHUNK(TimePixFile, '\n'); //skip header
#endif
#endif
	while (bShouldReadData || pixels.size() > 0)
	{
		int ret = 0;
#ifndef __SIMULATION__
		ret = katherine_acquisition_read(&acq_info);
#else
		size_t freeDataAmount = 0;
		do
		{
			ret = 0;
			if (!NOT_EOF(TimePixFile))
			{
				acq_info.state = ACQUISITION_TIMED_OUT;
				bShouldReadData = false;
			}

			if (bShouldReadData)
			{
				lineNb++;
#ifdef __USE_ADVACAM__
				READ_CHUNK(TimePixFile, '\t');	//Ignore Index
				std::stringstream s(READ_CHUNK(TimePixFile, '\t'));   //Matrix Index

				int MatrixIndex = 0;
				s >> MatrixIndex;

				std::stringstream s2(READ_CHUNK(TimePixFile, '\t'));   //Matrix Index
				uint64_t ToA = 0;
				s2 >> ToA;

				std::stringstream s3(READ_CHUNK(TimePixFile, '\t'));   //Matrix Index
				uint16_t ToT = 0;
				s3 >> ToT;

				std::stringstream s4(READ_CHUNK(TimePixFile, '\t'));   //Matrix Index
				uint8_t fToA = 0;
				s4 >> fToA;

				READ_CHUNK(TimePixFile, '\n');   //Jump to next line
#endif

#ifdef __USE_KATHERINE__
			// ###################  Katherine mode #################
				std::stringstream s(READ_CHUNK(TimePixFile, '\n'));

				std::string str = s.str();
				//is line commented out?
				if (stringChecker::checkIsLineCommented(str))
					continue;
				str.erase(std::remove_if(str.begin(), str.end(), [&](char c) {
					return !((c >= 48 && c <= 103) || (c == 9));
				}), str.end());
				s = std::stringstream(str);

				unsigned int MatrixIndex;
				s >> MatrixIndex;
				s >> pixel.toa;
				s >> pixel.tot;
				s >> pixel.ftoa;

				if (!pixel)
				{
					continue;
				}
#endif


				pixel.coord.x = MatrixIndex % 256;
				pixel.coord.y = MatrixIndex / 256;

				//Store pixel
				pixels.push_back(pixel);
			}

			//While is in sorting window
			freeDataAmount = isAllowedToWriteDataAt(CL_PixelBuffer->getWriteIndex(), pixels.size());
		} while (freeDataAmount <= 0 && bShouldReadData);

		
		acq_info.handlers.pixels_received(pixels.data(), std::min(freeDataAmount, pixels.size()));
		pixels.erase(pixels.begin(), pixels.begin() + std::min(freeDataAmount, pixels.size()));
#endif // !__SIMULATION__

		
		if (ret == 0)
		{
			if (bShouldReadData)
				OnNewDataRead(acq_info);
			else
				pixels.clear();
#ifndef __SIMULATION__
#ifdef WIN32
			Sleep(10);
#else
			nanosleep((const struct timespec[]) { {0, 10000000L} }, NULL);
#endif
#endif
		}
		else
		{
#ifdef WIN32
			Sleep(500);
#else
			nanosleep((const struct timespec[]) { {0, 500000000L} }, NULL);
#endif
		}
	}
}

size_t Tpx3DosageMeasurement::isAllowedToWriteDataAt(size_t begin, size_t amount)
{
	if (amount > (2*DataBufferSize)/3)
		amount = (2 * DataBufferSize) / 3;

	std::vector<size_t> calcSeg = getCalculationSegment();
	if (calcSeg[0] == 0 && calcSeg[1] == 0)
		return amount;

	size_t end = (begin + amount) % DataBufferSize;

	if (calcSeg[0] == 0 && calcSeg.size() == 3)
	{
		if (end < calcSeg[1] - amountOfToSortPixels)
			return amount;
	}

	if (begin < calcSeg[0] && end < calcSeg[0])
	{
		return amount;
	}
	else
	{
		if (begin >= calcSeg[1])
		{
			if (end < calcSeg[0])
				return amount;

			if (end > calcSeg[1])
				return amount;
		}
		else
		{
			if (begin < calcSeg[0])
				return calcSeg[0] - begin;

			if (begin < calcSeg[1] - amountOfToSortPixels)
				return calcSeg[1] - amountOfToSortPixels - begin;
		}
	}

	return 0;
}

FVector3D Tpx3DosageMeasurement::getRadiationDirection()
{
	return shadowAngle;
}

bool Tpx3DosageMeasurement::IsOperationPending()
{
	return ((bShouldReadData) || (SortDataThread->IsProcessing()) || (ClusterThread && ClusterThread->IsProcessing()) || PixelIntegratorThread->IsProcessing() || RadiationAngleReconstructor::IsOperationPending());
}

void Tpx3DosageMeasurement::setIntegrationTime(int ms)
{
	integrationTime_ms = ms;
}

uint64_t Tpx3DosageMeasurement::getElapsedProcessedTime_ns()
{
	return PixelIntegratorThread->getElapsed_ns();
}

float Tpx3DosageMeasurement::getHitsPerSecond()
{
	return PixelIntegratorThread->getHitsPerSecond();
}
