#pragma once
#include <KatherineTpx3.h>
#include "SegmentDataProcessingThread.h"
#include <functional>
#include "TimepixTypes.h"
#include "OpenCLTypes.h"
#include "GeometryTypes.h"
#include "MultiplattformTypes.h"

typedef std::function<void(OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>* list, size_t begin, size_t end)> FProcessingRawPixelEvent;
typedef std::function<void(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> image)> FProcessingIntegratedResultEvent;
typedef std::function<void(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> image, FVector3D& angles, FShadowSetup& shadows, uint64_t passedTime_ns)> FProcessingShadowResultEvent;
typedef std::function<void(std::shared_ptr<ClusterList> clusters)> FDosimetricEvent;

class PostprocessingThread : public SegmentDataProcessingThread
{
public:
	PostprocessingThread(size_t minAmount, OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>* list);
	~PostprocessingThread();

	void addProcessingMethod(FProcessingRawPixelEvent method, bool canBeExecutedIndependly = false);
	void addProcessingMethod(FProcessingIntegratedResultEvent method, bool canBeExecutedIndependly = false);
	void addProcessingMethod(FProcessingShadowResultEvent method, bool canBeExecutedIndependly = false);
	void addProcessingMethod(FDosimetricEvent method, bool canBeExecutedIndependly = false);

	static void registerNewIntegrationResult(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> image, uint64_t timeStamp);
	static void registerNewShadowResult(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> image, FVector3D angles, FShadowSetup& shadows);
	/** upto now only one data segment specified by the SegmentDataProcessingThread is allowed, so this method does nothing */
	static void registerNewDataSegment(size_t& begin, size_t& end);
	static void registerNewClusterList(std::shared_ptr<ClusterList>& clusters);


	virtual void Stop();

	virtual void EndProcessing(bool ignoreZombies) override;

protected:
	virtual void OnExecProcessing(size_t from, size_t to) override;
	virtual bool EvaluateSegmentReady(size_t& amount) override;

private:
	struct shadowArguments
	{
		std::shared_ptr<OCLMemoryVariable<cl::Image2D>> img;
		FVector3D& angles;
		FShadowSetup& shadows;
		uint64_t timeStamp_ns;

		shadowArguments(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> img, FVector3D& angles, FShadowSetup& shadows, uint64_t timeStamp_ns) : shadows(shadows), angles(angles)
		{
			this->img = img;
			this->timeStamp_ns = timeStamp_ns;
		}
	};

	std::vector<FProcessingRawPixelEvent> OutOfOrderExecutedRawMethods;
	std::vector<FProcessingRawPixelEvent> InOrderExecutedRawMethods;

	std::vector<FProcessingIntegratedResultEvent> OutOfOrderExecutedIntegratedMethods;
	std::vector<FProcessingIntegratedResultEvent> InOrderExecutedIntegratedMethods;

	std::vector<FProcessingShadowResultEvent> OutOfOrderExecutedShadowMethods;
	std::vector<FProcessingShadowResultEvent> InOrderExecutedShadowMethods;

	std::vector<FDosimetricEvent> OutOfOrderExecutedDosimetricMethods;
	std::vector<FDosimetricEvent> InOrderExecutedDosimetricMethods;

	OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>* pixelList = NULL;
	static std::vector<std::shared_ptr<OCLMemoryVariable<cl::Image2D>>> integrationResults;
	static std::vector<std::shared_ptr<shadowArguments>> shadowResults;
	static std::vector<std::pair<size_t, size_t>> DataSegments;
	static std::vector<std::shared_ptr<ClusterList>> clustersList;
	static std::vector<uint64_t> timeStampList;
	static MUTEXTYPE img_mutex;
	static MUTEXTYPE shadow_mutex;
	static MUTEXTYPE cluster_mutex;
};
