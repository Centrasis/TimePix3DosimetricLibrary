#pragma once 
#include <KatherineTpx3.h>
#ifndef __SIMULATION__
  #include <katherine/acquisition.h>
#else
  #include <TpxSimulation.h>
#endif
#include <string>
#include <MultiplattformTypes.h>
#include <opencv2/core/core.hpp>
#include "TimepixTypes.h"

#include "OpenCLTypes.h"
#include "GeometryTypes.h"

/** defines if clustering should be compiled into the library */
#define ENABLE_CLUSTERING

class PixelSorter;
class PixelIntegrator;

namespace std
{
	class thread;
}

class KatherineTimepix3;
class KatherineConfigParser;
class PixelClusterer;
class PostprocessingThread;

inline uint64_t ns2ms(uint64_t ns)
{
	return ns / 1000000;
}

class Tpx3DosageMeasurement
{
public:
#ifndef __SIMULATION__
	Tpx3DosageMeasurement();
#else
	Tpx3DosageMeasurement(std::string BaseFilePath);
#endif
	~Tpx3DosageMeasurement();

	bool searchTimepix(int lowerBound = 122, int upperBound = 126);
	bool useTimePix(KatherineTimepix3* timepix);
	KatherineTimepix3* getTimePix() { return usedTimepix; };

	bool isOnlineMeasurementActive();
	bool areTrustedSourcesActive();

	/** configures assigned timepix or if none is assigned createing a new one from the config file */
	bool configureTimepix(const char* path);
	bool doDataDrivenMode(KatherineConfigParser* config = nullptr);

	void abortDataReadout(bool ignoreZombies = false);

	PostprocessingThread* getPostProcessingThread();

	cv::Mat& getCurrentFrame();
	cv::Mat& getCurrentFrameWithFoundShadows();
#ifdef ENABLE_CLUSTERING
	cv::Mat& getCurrentFrameBlurred();
#endif

	void OnPixels_received(const void *pixels, size_t count);

	uint64_t getNewestToA() { return newestPixelToA; };
	size_t   getMaxWritingPos() { return beginOfCalculationSegment; };
	size_t   isAllowedToWriteDataAt(size_t begin, size_t amount);

	/** @returns if angle calc has returned it's first result */
	bool AnglesReady() { return gotFirstAngle; };

#ifdef __SIMULATION__
	void SetSimulationFileName(std::string name);
	void SetMaxProcessingTime_ns(uint64_t maxTime);
#endif

	FVector3D getRadiationDirection();

	bool IsOperationPending();

	void setIntegrationTime(int ms);

	/** Time in ms that was already integrated */
	uint64_t getElapsedProcessedTime_ns();
	float    getHitsPerSecond();

	void setFilterSetup(std::string & path, unsigned int px_width, unsigned int px_height);
	FFilterSetup getFilterSetup();

protected:
	unsigned int integrationTime_ms = 12;
	unsigned int minimalEventsPerFrame = 12000;
	uint64_t latestPixelTime = 0;
#ifdef __SIMULATION__
	//in [ns]
	uint64_t maximalPixelTimeForReading = 0;
#endif
	KatherineTimepix3* usedTimepix = nullptr;
	KatherineConfigParser* configParser = nullptr;
	katherine_acquisition_t acq_info;
	std::thread* ReadDataThread = nullptr;
	PixelSorter* SortDataThread = nullptr;
	PixelClusterer* ClusterThread = nullptr;
	PixelIntegrator* PixelIntegratorThread = nullptr;
	PostprocessingThread* postProcessingThread = nullptr;
	virtual void OnNewDataRead(katherine_acquisition_t& acq);
	MUTEXTYPE lock;
	cv::Mat currentFrameBuffer;
	cv::Mat shadowFrame;
#ifdef __USE_OPENCL__
	OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>* CL_PixelBuffer;
#endif
	const size_t DataBufferSize = PixelDataBufferSize;
	//these last pixels could be out of order -> sort!
	const size_t amountOfToSortPixels = 50;
	const size_t MinAmountOfClusteringPixels = 500;
	FFilterSetup TpxFilters;
	std::string basePath;

#ifdef ENABLE_CLUSTERING
	cv::Mat blurredFrame;
#endif

	void OnIntegrationFinished(
#ifdef __USE_OPENCL__
		std::shared_ptr<OCLMemoryVariable<cl::Image2D>> integrationResult,
#endif
		uint64_t timeStamp
	);
	void OnAngleCalcFinished(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> image, FVector3D& angles, FShadowSetup& shadows);

	void addPixel(katherine_px_f_toa_tot_t* pixel);

	void correctPixelToA(katherine_px_f_toa_tot_t * pixel);

	void OnNewPixelSorted(size_t begin, size_t end);

	void OnNewClustersFound(std::shared_ptr<ClusterList> clusters);

	std::vector<size_t> getCalculationSegment();

	/** updates the currentFrameBuffer witch holds the integral
	void updateIntegralOfPixels();*/

private:
#ifdef __SIMULATION__
	FILETYPE_IN TimePixFile = NULL;
#endif
	uint64_t newestPixelToA = 0;
	size_t beginOfCalculationSegment = DataBufferSize;
	//Max seen Value
	const uint64_t ToAOverflowValue = TPX_ROLLOVER_VALUE;
	uint64_t currentToAInterval = 0;
	int rolloverCount = 0;
	bool bShouldReadData = false;
	void ExecReadData();
	FVector3D shadowAngle = 0.f;
	bool isCalcingAngle = false;
	bool gotFirstAngle = false;

	int itr = 0;
};