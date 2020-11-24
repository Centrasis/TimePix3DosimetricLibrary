#include "PostprocessingThread.h"

std::vector<std::shared_ptr<OCLMemoryVariable<cl::Image2D>>> PostprocessingThread::integrationResults;
std::vector<std::shared_ptr<PostprocessingThread::shadowArguments>> PostprocessingThread::shadowResults;
MUTEXTYPE CREATEMUTEX(PostprocessingThread::img_mutex);
MUTEXTYPE CREATEMUTEX(PostprocessingThread::shadow_mutex);
MUTEXTYPE CREATEMUTEX(PostprocessingThread::cluster_mutex);
std::vector<std::shared_ptr<ClusterList>> PostprocessingThread::clustersList;
std::vector<uint64_t> PostprocessingThread::timeStampList;

PostprocessingThread::PostprocessingThread(size_t minAmount, OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>* list) : SegmentDataProcessingThread(list->getSize(), 0, minAmount, 0)
{
	pixelList = list;
}


PostprocessingThread::~PostprocessingThread()
{
	InOrderExecutedRawMethods.clear();
	OutOfOrderExecutedRawMethods.clear();
}

void PostprocessingThread::addProcessingMethod(FProcessingRawPixelEvent method, bool canBeExecutedIndependly)
{
	if (canBeExecutedIndependly)
		OutOfOrderExecutedRawMethods.push_back(method);
	else
		InOrderExecutedRawMethods.push_back(method);
}

void PostprocessingThread::addProcessingMethod(FProcessingIntegratedResultEvent method, bool canBeExecutedIndependly)
{
	if (canBeExecutedIndependly)
		OutOfOrderExecutedIntegratedMethods.push_back(method);
	else
		InOrderExecutedIntegratedMethods.push_back(method);
}

void PostprocessingThread::addProcessingMethod(FProcessingShadowResultEvent method, bool canBeExecutedIndependly)
{
	if (canBeExecutedIndependly)
		OutOfOrderExecutedShadowMethods.push_back(method);
	else
		InOrderExecutedShadowMethods.push_back(method);
}

void PostprocessingThread::addProcessingMethod(FDosimetricEvent method, bool canBeExecutedIndependly)
{
	if (canBeExecutedIndependly)
		OutOfOrderExecutedDosimetricMethods.push_back(method);
	else
		InOrderExecutedDosimetricMethods.push_back(method);
}

void PostprocessingThread::registerNewIntegrationResult(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> image, uint64_t timeStamp)
{
	ACQUIRE_MUTEX(img_mutex);
	PostprocessingThread::integrationResults.push_back(image);
	PostprocessingThread::timeStampList.push_back(timeStamp);
	RELEASE_MUTEX(img_mutex);
}

void PostprocessingThread::registerNewShadowResult(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> image, FVector3D angles, FShadowSetup & shadows)
{
	ACQUIRE_MUTEX(shadow_mutex);
	uint64_t tstamp;
	ACQUIRE_MUTEX(img_mutex);
	if (PostprocessingThread::timeStampList.size() == 0)
	{
		printf("no image should be left!\n");
		RELEASE_MUTEX(img_mutex);
		RELEASE_MUTEX(shadow_mutex);
		return;
	}
	tstamp = PostprocessingThread::timeStampList[0];
	PostprocessingThread::timeStampList.erase(PostprocessingThread::timeStampList.begin(), PostprocessingThread::timeStampList.begin() + 1);
	RELEASE_MUTEX(img_mutex);
	PostprocessingThread::shadowResults.push_back(std::make_shared<shadowArguments>(image, angles, shadows, tstamp));
	RELEASE_MUTEX(shadow_mutex);
}

void PostprocessingThread::registerNewDataSegment(size_t& begin, size_t& end)
{
	/*segment_mutex.lock();
	DataSegments.push_back(std::pair<size_t, size_t>(begin, end));
	segment_mutex.unlock();*/
}

void PostprocessingThread::registerNewClusterList(std::shared_ptr<ClusterList>& clusters)
{
	ACQUIRE_MUTEX(PostprocessingThread::cluster_mutex);	
	PostprocessingThread::clustersList.push_back(clusters);
	RELEASE_MUTEX(PostprocessingThread::cluster_mutex);
}

void PostprocessingThread::Stop()
{
	ACQUIRE_MUTEX(PostprocessingThread::cluster_mutex);
	ACQUIRE_MUTEX(shadow_mutex);
	ACQUIRE_MUTEX(img_mutex);

	shadowResults.clear();
	integrationResults.clear();
	clustersList.clear();

	RELEASE_MUTEX(PostprocessingThread::cluster_mutex);
	RELEASE_MUTEX(shadow_mutex);
	RELEASE_MUTEX(img_mutex);
}

void PostprocessingThread::EndProcessing(bool ignoreZombies)
{
	Stop();
	SegmentDataProcessingThread::EndProcessing(ignoreZombies);
}

void PostprocessingThread::OnExecProcessing(size_t from, size_t to)
{
	std::vector<std::thread> runningAsyncThreads;
	for (int i = 0; i < OutOfOrderExecutedRawMethods.size(); i++)
	{
		runningAsyncThreads.push_back(std::thread(OutOfOrderExecutedRawMethods[i], pixelList, from, to));
	}

	for (int i = 0; i < InOrderExecutedRawMethods.size(); i++)
	{
		std::thread currentInOrderThread = std::thread(InOrderExecutedRawMethods[i], pixelList, from, to);
		currentInOrderThread.join();
	}

	for (int i = 0; i < runningAsyncThreads.size(); i++)
	{
		runningAsyncThreads[i].join();
	}
}

bool PostprocessingThread::EvaluateSegmentReady(size_t & amount)
{
	if (OutOfOrderExecutedIntegratedMethods.size() > 0 || InOrderExecutedIntegratedMethods.size() > 0)
	{
		ACQUIRE_MUTEX(img_mutex);
		if (integrationResults.size() > 0)
		{
			std::shared_ptr<OCLMemoryVariable<cl::Image2D>> img = integrationResults[0];
			integrationResults.erase(integrationResults.begin());
			RELEASE_MUTEX(img_mutex);

			std::vector<std::thread> runningAsyncThreads;
			for (int i = 0; i < OutOfOrderExecutedIntegratedMethods.size(); i++)
			{
				runningAsyncThreads.push_back(std::thread(OutOfOrderExecutedIntegratedMethods[i], img));
			}

			for (int i = 0; i < InOrderExecutedIntegratedMethods.size(); i++)
			{
				std::thread currentInOrderThread = std::thread(InOrderExecutedIntegratedMethods[i], img);
				currentInOrderThread.join();
			}

			for (int i = 0; i < runningAsyncThreads.size(); i++)
			{
				runningAsyncThreads[i].join();
			}
		}
		else
			RELEASE_MUTEX(img_mutex);
	}
	else
	{
		if (integrationResults.size() > 0)
		{
			ACQUIRE_MUTEX(PostprocessingThread::img_mutex);
			integrationResults.clear();
			RELEASE_MUTEX(PostprocessingThread::img_mutex);
		}
	}
	//FVector3D angles, FShadowSetup shadows, float hitsPerSecond, uint64_t passedTime_ns, std::shared_ptr<ClusterList> clusters
	if (OutOfOrderExecutedDosimetricMethods.size() > 0 || InOrderExecutedDosimetricMethods.size() > 0)
	{
		ACQUIRE_MUTEX(PostprocessingThread::cluster_mutex);
		if (clustersList.size() > 0)
		{
			std::shared_ptr<ClusterList> clusterInfo = clustersList[0];
			clustersList.erase(clustersList.begin());
			RELEASE_MUTEX(PostprocessingThread::cluster_mutex);

			std::vector<std::thread> runningAsyncThreads;
			for (int i = 0; i < OutOfOrderExecutedDosimetricMethods.size(); i++)
			{
				runningAsyncThreads.push_back(std::thread(OutOfOrderExecutedDosimetricMethods[i], clusterInfo));
			}

			for (int i = 0; i < InOrderExecutedDosimetricMethods.size(); i++)
			{
				std::thread currentInOrderThread = std::thread(InOrderExecutedDosimetricMethods[i], clusterInfo);
				currentInOrderThread.join();
			}

			for (int i = 0; i < runningAsyncThreads.size(); i++)
			{
				runningAsyncThreads[i].join();
			}
		}
		else
			RELEASE_MUTEX(PostprocessingThread::cluster_mutex);
	}
	else
	{
		if (clustersList.size() > 0)
		{
			ACQUIRE_MUTEX(PostprocessingThread::cluster_mutex);
			clustersList.clear();
			RELEASE_MUTEX(PostprocessingThread::cluster_mutex);
		}
	}

	if (OutOfOrderExecutedShadowMethods.size() > 0 || InOrderExecutedShadowMethods.size() > 0)
	{
		ACQUIRE_MUTEX(shadow_mutex);
		if (shadowResults.size() > 0)
		{
			std::shared_ptr<shadowArguments> s = shadowResults[0];
			RELEASE_MUTEX(shadow_mutex);

			std::vector<std::thread> runningAsyncThreads;
			for (int i = 0; i < OutOfOrderExecutedShadowMethods.size(); i++)
			{
				runningAsyncThreads.push_back(std::thread(OutOfOrderExecutedShadowMethods[i], s->img, std::ref(s->angles), std::ref(s->shadows), s->timeStamp_ns));
			}

			for (int i = 0; i < InOrderExecutedShadowMethods.size(); i++)
			{
				std::thread currentInOrderThread = std::thread(InOrderExecutedShadowMethods[i], s->img, std::ref(s->angles), std::ref(s->shadows), s->timeStamp_ns);
				currentInOrderThread.join();
			}

			for (int i = 0; i < runningAsyncThreads.size(); i++)
			{
				runningAsyncThreads[i].join();
			}

			ACQUIRE_MUTEX(shadow_mutex);
			shadowResults.erase(shadowResults.begin());
			RELEASE_MUTEX(shadow_mutex);
		}
		else
			RELEASE_MUTEX(shadow_mutex);
	}
	else
	{
		shadowResults.clear();
	}
	
	return SegmentDataProcessingThread::EvaluateSegmentReady(amount);
}
