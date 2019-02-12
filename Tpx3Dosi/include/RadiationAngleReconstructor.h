#pragma once
#include "KatherineTpx3.h"
#include <GeometryTypes.h>
#include <opencv/cv.hpp>
#include <functional>
#include "OpenCLTypes.h"
#include "MultiplattformTypes.h"
#include <random>
#include <vector>

typedef std::function<void(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> image, FVector3D& angles, FShadowSetup& shadows)> FOnAngleCalcFinished;

class RadiationAngleReconstructor
{
public:
	RadiationAngleReconstructor();
	RadiationAngleReconstructor(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> FrameBuffer);
	virtual ~RadiationAngleReconstructor();
	static void SetFilterSetup(FFilterSetup filters);
	bool LoadSetupFromArchive(std::string path);
	/** calculates the angle of the rays that produced the frame buffer given that the rays are parallel */
	void calcParallelInRayAngle(FOnAngleCalcFinished onAngleFinished = NULL);
	static FShadowSetup getShadowSetup();
	static size_t MaxThreadCount;
	static size_t TestIterations;
	static unsigned int precision;
	static float ShadowThreshold;
	static float MaxPercentageOverThreshold;
	static void AbortCalc();
	static float AngleRange;
	static char RepeatsCount;
	static bool IsOperationPending();
	static float previousFoundRotation[2];
	static float currentScore;

protected:
	static FOCLKernel BaseFilterSetupRatioCalcKernel;
	static FOCLKernel BaseOverallShadowRatioCalcKernel; 
	static FOCLKernel BaseCalcShadowsFromFilterSetup;
	static FFilterSetup FilterSetup;
	static MUTEXTYPE staticInnerLock;
	static MUTEXTYPE staticOuterLock;
	static std::random_device realRandom;
	static std::default_random_engine pseudoRandom;
	//static OCLTypedVariable<float, ASPrivate, 2> radiationAngle;
	static unsigned int getNextRandom(unsigned int bound = (unsigned int)(std::pow(2, 30) + 1) * 2);
	void calcRadiationAngle(FOnAngleCalcFinished onAngleFinished, std::vector<FShadow> shadows);
	static FVector3D radiationAngleResult;
	static FShadowSetup resultShadowSetup;
	FVector3D radiationVector;
	void ExecCalculation();
	FOnAngleCalcFinished onAngleCalcFinished;
	std::shared_ptr<OCLMemoryVariable<cl::Image2D>> integrationResult = NULL;

private:
	char repeatCount = 0;
	static bool firstConstructorCall;
	static void ExecSelectCalculationResult();
	static std::vector<RadiationAngleReconstructor*> waitingCalcs;
};
