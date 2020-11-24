#include "PixelClusterer.h"
#ifdef __USE_SYCL__
#include <CL/sycl.hpp>
#endif
#include "OpenCLExecutor.h"
#include "opencv2/core.hpp"

#ifdef __USE_CLUSTERER_OPENCL
PixelClusterer::PixelClusterer(FClusteringMethod ClusteringAlgo, OCLVariable* pixellist, const size_t amountOfMinToClusterPixels, const size_t BufferSize,
	std::function<void(ClusterList* clusters)> OnClusteringFinish, float sigma)
		: SegmentDataProcessingThread::SegmentDataProcessingThread(BufferSize, 0, amountOfMinToClusterPixels, 0)
{
	CREATEMUTEX(lock);

	this->ClusteringAlgo = ClusteringAlgo;
	this->OnClusteringFinish = OnClusteringFinish;

	gaussianKernel = new FOCLKernel(loadOCLKernel(PixelClustering, "gaussianBlur"));

	minClusteringSementSize = amountOfMinToClusterPixels;
	pixels = pixellist;

	blurredImg = (std::shared_ptr<OCLMemoryVariable<cl::Image2D>>*) malloc(2 * sizeof(std::shared_ptr<OCLMemoryVariable<cl::Image2D>>));
	blurredImg[0] = NULL;
	blurredImg[1] = NULL;

	OCLMask = (OCLDynamicTypedBuffer<float>**) malloc(2 * sizeof(OCLDynamicTypedBuffer<float>*));
	for (int i = 0; i < 2; i++)
		OCLMask[i] = new OCLDynamicTypedBuffer<float>();

	//Init Kernel with mostly placeholders
	gaussianKernel->Arguments.push_back(NULL);
	gaussianKernel->Arguments.push_back(NULL);
	gaussianKernel->Arguments.push_back(NULL);
	gaussianKernel->Arguments.push_back(&OCLMaskSize);
}

PixelClusterer::~PixelClusterer()
{
	delete[] blurredImg;
	delete[] OCLMask;
	delete gaussianKernel;
}

void PixelClusterer::setWorkImage(cv::Mat & img)
{
	WorkImage = &img;
}

void PixelClusterer::addIntegrationResult(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> integrationResult)
{
	if (blurredImg[0] != NULL)
	{
		return;
	}

	size_t width = integrationResult->getTypedValue()->getImageInfo<CL_IMAGE_WIDTH>();
	size_t height = integrationResult->getTypedValue()->getImageInfo<CL_IMAGE_HEIGHT>();

	cl_int err;
	blurredImg[1] = new OCLMemoryVariable<cl::Image2D>(cl::Image2D(OpenCLExecutor::getExecutor().getContext(), EOCLAccessTypes::ATReadWrite, cl::ImageFormat(CL_R, CL_UNSIGNED_INT8), width, height, 0, NULL, &err), "", true, ATReadWrite);
	if (err != CL_SUCCESS)
	{
		std::printf("Could not create blur image on GPU: %s\n", OpenCLExecutor::decodeErrorCode(err).c_str());
		throw OCLException("Could not create blur image on GPU");
	}
	blurredImg[1]->setHostPointerMode(ATWrite);
	blurredImg[1]->setHostPointer(NULL);

	blurredImg[0] = new OCLMemoryVariable<cl::Image2D>(cl::Image2D(OpenCLExecutor::getExecutor().getContext(), EOCLAccessTypes::ATReadWrite, cl::ImageFormat(CL_R, CL_UNSIGNED_INT8), width, height, 0, NULL, &err), "", true, ATReadWrite);
	if (err != CL_SUCCESS)
	{
		std::printf("Could not create blur image on GPU: %s\n", OpenCLExecutor::decodeErrorCode(err).c_str());
		throw OCLException("Could not create blur image on GPU");
	}
	blurredImg[0]->setHostPointerMode(ATWrite);
	blurredImg[0]->setHostPointer(WorkImage->data);

	for (int i = 0; i < 2; i++)
	{
		float sigma = sigmas[i];
		int maskSize = (int)ceil(3.0f*sigma);
		float* mask = new float[(maskSize * 2 + 1)*(maskSize * 2 + 1)];
		OCLMask[i]->resizeBuffer(maskSize);
		OCLMask[i]->setValue(mask);
		delete[] mask;
		float sum = 0.0f;
		for (int a = -maskSize; a < maskSize + 1; a++) {
			for (int b = -maskSize; b < maskSize + 1; b++) {
				float temp = exp(-((float)(a*a + b * b) / (2 * sigma*sigma)));
				sum += temp;
				(*OCLMask[i])[a + maskSize + (b + maskSize)*(maskSize * 2 + 1)] = temp;
			}
		}
		// Normalize the mask
		for (int j = 0; j < (maskSize * 2 + 1)*(maskSize * 2 + 1); j++)
			(*OCLMask[i])[j] = (*OCLMask[i])[j] / sum;
	}

	gaussianKernel->globalThreadCount = cl::NDRange(width, height);
	gaussianKernel->localCount = OpenCLExecutor::getExecutor().getMaxLocalNDRange(gaussianKernel->globalThreadCount);

	integrationResults.push_back(integrationResult);
}

void PixelClusterer::UpdateFrame()
{
	ACQUIRE_MUTEX(lock);
#ifdef __USE_OPENCL__
	OpenCLExecutor::getExecutor().GetResultOf(*gaussianKernel, blurredImg[1]);
#endif
	RELEASE_MUTEX(lock);
}

bool PixelClusterer::EvaluateSegmentReady(size_t & amount)
{
	return integrationResults.size() > 0;
}

void PixelClusterer::OnExecProcessing(size_t from, size_t to)
{
	clusters = new ClusterList();
	bool retValOfClustering = false;
#ifdef __USE_OPENCL__
	#ifdef __USE_SYCL__
		/*cl::sycl::queue deviceQueue;
		cl::sycl::buffer<size_t> retBufferLastClusterPosition(1);
		cl::sycl::buffer<ClusterList> ClusterBuffer(clusters, cl::sycl::range<1>());
		std::vector<FClusterPositionLinker> pixelVector;
		for (size_t i = from; i <= to; i++)
		{
			pixelVector[i].px = *(*pixels)[i];
			pixelVector[i].positionInList = i;
		}
		cl::sycl::buffer<FClusterPositionLinker> PixelBuffer(pixelVector.begin(), pixelVector.end());
		PixelBuffer.set_write_back(false);

		//cl::sycl::buffer<bool> retValueOfClusteringBuffer(retValOfClustering);
		//retValueOfClusteringBuffer.set_write_back(true);

	
		deviceQueue.submit([&](cl::sycl::handler& chg)
		{
			FGlobalBufferAccess<size_t, SYCL_READ_WRITE> retAccessor = retBufferLastClusterPosition.get_access<SYCL_READ_WRITE>(chg);
			FGlobalBufferAccess<FClusterPositionLinker, SYCL_READ_WRITE> pixelAccess = PixelBuffer.get_access<SYCL_READ_WRITE>(chg);
			FGlobalBufferAccess<ClusterList, SYCL_WRITE> clusterListAccess = ClusterBuffer.get_access<SYCL_WRITE>(chg);
			//FGlobalBufferAccess<bool, SYCL_WRITE> retValueOfClusteringAccess = retValueOfClusteringBuffer.get_access<SYCL_WRITE>(chg);

			if (ClusteringAlgo(pixelAccess, clusterListAccess, retAccessor))
			{
				retValOfClustering = true;
				processingStepWidth = 1 + lastClusterEndPosition - from;
				OnClusteringFinish(clusters);
			}
			else
			{
				RevertProcessingStep();
			}
		});*/
    #else
	
	ACQUIRE_MUTEX(lock);

	for (int i = 0; i < 2; i++)
	{
		gaussianKernel->Arguments[1] = blurredImg[i];
		int maskSize = (int)ceil(3.0f*sigmas[i]);
		gaussianKernel->Arguments[0] = integrationResults[0];
		OCLMaskSize[0] = maskSize;
		OCLMaskSize.setVariableChanged(true);
		gaussianKernel->Arguments[2] = OCLMask[i];

		OpenCLExecutor::getExecutor().RunKernel(*gaussianKernel);
	}
	integrationResults.erase(integrationResults.begin());
	RELEASE_MUTEX(lock);

	#endif
#else
	retValOfClustering = ClusteringAlgo(*pixels, from, to, *clusters, lastClusterEndPosition);
	if (retValOfClustering)
	{
		processingStepWidth = 1 + lastClusterEndPosition - from;
		OnClusteringFinish(clusters);
	}
	else
	{
		RevertProcessingStep();
	}
#endif

	//must be outsourced, because it can't be done by any OpenCL device
	if(!retValOfClustering)
		delete clusters;
}
#else
PixelClusterer::PixelClusterer(FClusteringMethod ClusteringAlgo, OCLVariable* pixellist, const size_t amountOfMinToClusterPixels, const size_t BufferSize,
	std::function<void(std::shared_ptr<ClusterList> clusters)> OnClusteringFinish, float sigma)
	: SegmentDataProcessingThread::SegmentDataProcessingThread(BufferSize, 0, amountOfMinToClusterPixels, 0)
{
	this->ClusteringAlgo = ClusteringAlgo;
	this->OnClusteringFinish = OnClusteringFinish;

	pixels = pixellist;
}

PixelClusterer::~PixelClusterer()
{
	
}

void PixelClusterer::setWorkImage(cv::Mat & img)
{
	
}

void PixelClusterer::addIntegrationResult(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> integrationResult)
{
}

void PixelClusterer::UpdateFrame()
{
}

bool PixelClusterer::EvaluateSegmentReady(size_t & amount)
{
	return SegmentDataProcessingThread::EvaluateSegmentReady(amount);
}

void PixelClusterer::OnExecProcessing(size_t from, size_t to)
{
	clusters = std::make_shared<ClusterList>();
	bool retValOfClustering = false;

	retValOfClustering = ClusteringAlgo((OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>*)pixels, from, to, clusters, lastClusterEndPosition);
	if (retValOfClustering)
	{
		processingStepWidth = 1 + lastClusterEndPosition - from;
		OnClusteringFinish(clusters);
	}
	else
	{
		RevertProcessingStep();
	}
}
#endif