#include "PixelIntegrator.h"
#include "TimepixTypes.h"
#ifdef __USE_OPENCL__
#include "TimepixDataCLProc.h"
#endif
#include "RadiationAngleReconstructor.h"

PixelIntegrator::PixelIntegrator(OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>* pixellist, uint64_t integrationTime, const size_t BufferSize) : SegmentDataProcessingThread::SegmentDataProcessingThread(BufferSize, 0, 200, 0)
{
	this->integrationTime = integrationTime;
	CREATEMUTEX(lock);
#ifdef __USE_OPENCL__
	//((OpenCLExecutor*)(&TimepixDataCLProc::getExecutor()))->InitKernel(PixelIntegrateKernel);

	CL_PixelBuffer = pixellist;
	CL_BufferLowAddress = OCLTypedVariable<size_t, ASPrivate>((size_t)0, "", true, ATRead);
	CL_BufferHighAddress = OCLTypedVariable<size_t, ASPrivate>((size_t)0, "", true, ATRead);
	cl_int err = CL_SUCCESS;
	img_width = 256;
	img_height = 256;
	imgData = NULL;
	//CL_IntegratedPixels = OCLMemoryVariable<cl::Image2D>(cl::Image2D(((OpenCLExecutor*)(&TimepixDataCLProc::getExecutor()))->getContext(), EOCLAccessTypes::ATReadWrite, cl::ImageFormat(CL_R, CL_UNSIGNED_INT8), 256, 256, 0, NULL, &err), "", true, ATReadWrite);
	CL_Iteration = OCLTypedVariable<int>(0, "", true, ATRead);
	if (err != CL_SUCCESS)
	{
		throw OCLException("CL ERROR:" + OpenCLExecutor::decodeErrorCode(err));
	}
	PixelIntegrateKernel.globalThreadCount = cl::NDRange(48);

	PixelIntegrateKernel.Arguments.push_back(CL_PixelBuffer);
	PixelIntegrateKernel.Arguments.push_back(&CL_BufferLowAddress);
	PixelIntegrateKernel.Arguments.push_back(&CL_BufferHighAddress);
	PixelIntegrateKernel.Arguments.push_back(&CL_PixelBufferLen);
	PixelIntegrateKernel.Arguments.push_back(NULL);	//placeholder for CL image object pointer

	backgroundColor.x = 0;
	backgroundColor.y = 0;
	backgroundColor.z = 0;
	backgroundColor.w = 255;

#ifdef USE_BLUR
	//Init gaussian Blur Mask
	for (int i = 0; i < 2; i++)
	{
		int maskSize = (int)ceil(3.0f*sigma);
		float* mask = new float[(maskSize * 2 + 1)*(maskSize * 2 + 1)];
		OCLMask.resizeBuffer(maskSize);
		OCLMask.setValue(mask);
		delete[] mask;
		float sum = 0.0f;
		for (int a = -maskSize; a < maskSize + 1; a++) {
			for (int b = -maskSize; b < maskSize + 1; b++) {
				float temp = exp(-((float)(a*a + b * b) / (2 * sigma*sigma)));
				sum += temp;
				OCLMask[a + maskSize + (b + maskSize)*(maskSize * 2 + 1)] = temp;
			}
		}
		// Normalize the mask
		for (int j = 0; j < (maskSize * 2 + 1)*(maskSize * 2 + 1); j++)
			OCLMask[j] = OCLMask[j] / sum;
	}

	//Init Kernel with mostly placeholders
	gaussianKernel.Arguments.push_back(NULL);
	gaussianKernel.Arguments.push_back(NULL);
	gaussianKernel.Arguments.push_back(&OCLMask);
	gaussianKernel.Arguments.push_back(&OCLMaskSize);
	OCLMaskSize[0] = (int)ceil(3.0f*sigma);

	//copyKernel.Arguments.push_back(NULL);
	//copyKernel.Arguments.push_back(NULL);
#endif

#endif
}


PixelIntegrator::~PixelIntegrator()
{
#ifdef __USE_OPENCL__
	((OpenCLExecutor*)(&TimepixDataCLProc::getExecutor()))->ReleaseKernel(PixelIntegrateKernel);
	DESTROYMUTEX(lock);
	SegmentDataProcessingThread::~SegmentDataProcessingThread();
#endif
}

void PixelIntegrator::SetOnIntegrationFinished(FOnIntegrationFinished onFinished)
{
	OnIntegrationFinished = onFinished;
}

void PixelIntegrator::SetFrameBuffer(cv::Mat & img)
{
#ifdef __USE_OPENCL__
	img_width = img.cols;
	img_height = img.rows;
	imgData = img.data;
#ifdef USE_BLUR
	//blurredImg = OCLMemoryVariable<cl::Image2D>(cl::Image2D(OpenCLExecutor::getExecutor().getContext(), EOCLAccessTypes::ATReadWrite, cl::ImageFormat(CL_R, CL_UNSIGNED_INT8), img_width, img_height), "", true, ATReadWrite);
	//gaussianKernel.Arguments[1] = &blurredImg;
#endif
#endif
}

void PixelIntegrator::UpdateFrame(bool shouldReadBlocking)
{
	ACQUIRE_MUTEX(lock);
#ifdef __USE_OPENCL__
	if (CL_IntegratedPixels.size() < 2)
	{
		RELEASE_MUTEX(lock);
		return;
	}
	//((OpenCLExecutor*)(&TimepixDataCLProc::getExecutor()))->GetResultOf(PixelIntegrateKernel, CL_IntegratedPixels[CL_IntegratedPixels.size() - 2]);

	auto q = OpenCLExecutor::getExecutor().createQueue();
	CL_IntegratedPixels[CL_IntegratedPixels.size() - 2]->downloadBuffer(&q);

	if (shouldReadBlocking)
		((OpenCLExecutor*)(&TimepixDataCLProc::getExecutor()))->WaitForKernel(PixelIntegrateKernel);
#endif
	RELEASE_MUTEX(lock);
}

uint64_t PixelIntegrator::getElapsed_ms()
{
	return (uint64_t)getElapsed_ns() / 1000000;
}

uint64_t PixelIntegrator::getElapsed_ns()
{
	return elapsedTime_ns;
}

float PixelIntegrator::getHitsPerSecond()
{
	return hitsPerSecond;
}

void PixelIntegrator::setMinimalEventsPerFrame(uint64_t count)
{
	minimalEventsPerFrame = count;
}

bool PixelIntegrator::EvaluateSegmentReady(size_t & amount)
{
	size_t lowerPos = GetCurrentProcessingPos();
	size_t upperPos = GetUpperProcessingConstraint();

	if (upperPos < lowerPos)
		upperPos += ((size_t)(CL_PixelBuffer->getSize() / CL_PixelBuffer->getTypeSize()));

	if (upperPos > 0)
		upperPos--;

	if (upperPos - lowerPos < GetMinProcessingAmount())
	{
		amount = 0;
		processingStepWidth = 1;
		return false;
	}

	auto highPx = (*CL_PixelBuffer)[upperPos];
	auto lowPx  = (*CL_PixelBuffer)[lowerPos];

	if (!highPx.IsValid())
	{
		return false;
	}

	if (highPx.toa < lowPx.toa)
	{
		size_t itr = 0;
		while (lowerPos + itr <= upperPos && highPx.toa < lowPx.toa)
		{
			lowPx = (*CL_PixelBuffer)[lowerPos + itr];
			itr = itr + 1;
		}
		if (lowerPos + itr > upperPos && highPx.toa < lowPx.toa)
		{
#ifdef SHOW_TPX_ERRORS
			//std::printf("To many pixels per time registered! Increase RingBuffer size or reduce radiation! [Buffer could hold %I64u ns]\n", (highPx.toa - lowPx.toa));
#endif
			return false;
		}
		//CL_PixelBuffer->startNewDataBuffer();
		//processingStepWidth = 1;
		//return false;
	}
	
	if (!lowPx)
		return false;

	uint64_t elapsedTime = (highPx.toa - lowPx.toa);
	uint64_t deltaPos = upperPos - lowerPos;
	if (lowerPos < upperPos)
	{

		hitsPerSecond = ((float)(deltaPos) / (float)elapsedTime) * 1000000000.f;
	}
	else
	{
		hitsPerSecond = (((float)PixelDataBufferSize - lowerPos + upperPos) / elapsedTime) * 1000000000.f;
	}

	if (minimalEventsPerFrame <= deltaPos)
	{
		long long diff = upperPos - lowerPos;
		if(diff >= 0)
			amount = diff;
		else
		{
			amount = MaxBufferSize - lowerPos;
			amount += upperPos;
		}

		processingStepWidth = amount;
		return true;
	}
	else
	{
		/*
		//update integration interval
		if (minimalEventsPerFrame > 0 && hitsPerSecond > -1.f)
		{
			if (integrationTime * hitsPerSecond / 1000000000.f < minimalEventsPerFrame)
			{
				float maxEventsPerSecond = hitsPerSecond / minimalEventsPerFrame;
				integrationTime = (uint64_t)(1000000000.f / maxEventsPerSecond);
			}
		}
		*/
	}

	return false;
}

void PixelIntegrator::OnExecProcessing(size_t from, size_t to)
{
#ifdef __USE_OPENCL__
	ACQUIRE_MUTEX(lock);

	cl_int err = CL_SUCCESS;
	if (CL_IntegratedPixels.size() >= maxImageBuffers)
	{
		CL_IntegratedPixels.erase(CL_IntegratedPixels.begin(), CL_IntegratedPixels.begin() + 1);
	}
	std::shared_ptr<OCLMemoryVariable<cl::Image2D>> integratedPixels = std::make_shared<OCLMemoryVariable<cl::Image2D>>(cl::Image2D(((OpenCLExecutor*)(&TimepixDataCLProc::getExecutor()))->getContext(), EOCLAccessTypes::ATReadWrite, cl::ImageFormat(CL_R, CL_UNSIGNED_INT8), img_width, img_height, 0, NULL, &err), "", true, ATReadWrite);
	CL_IntegratedPixels.push_back(integratedPixels);
	integratedPixels->setHostPointerMode(ATWrite);
	integratedPixels->setHostPointer(imgData);
	PixelIntegrateKernel.Arguments[4] = *integratedPixels;

	if (err != CL_SUCCESS)
	{
		throw OCLException("CL ERROR: " + OpenCLExecutor::decodeErrorCode(err));
	}

	OpenCLExecutor::getExecutor().InitOCLVariable(*integratedPixels, &backgroundColor);

	if (bIsFirstCall)
	{
		bIsFirstCall = false;
		beginTime = (*CL_PixelBuffer)[from].toa;
	}

	uint64_t temp = (*CL_PixelBuffer)[to].toa - beginTime;
	if (elapsedTime_ns != temp)
		elapsedTime_ns = temp;
	else
	{
		return;
	}

	CL_BufferLowAddress[0] = from;
	CL_BufferHighAddress[0] = to % CL_PixelBufferLen[0];

	if (!((OpenCLExecutor*)(&TimepixDataCLProc::getExecutor()))->RunKernel(PixelIntegrateKernel))
		throw "Can't integrate pixels!";

	((OpenCLExecutor*)(&TimepixDataCLProc::getExecutor()))->WaitForKernel(PixelIntegrateKernel);
#ifdef USE_BLUR
	gaussianKernel.Arguments[0] = integratedPixels;
	gaussianKernel.Arguments[1] = integratedPixels;
	OpenCLExecutor::getExecutor().RunKernel(gaussianKernel);
	OpenCLExecutor::getExecutor().WaitForKernel(gaussianKernel);

	/*copyKernel.Arguments[0] = &blurredImg;
	copyKernel.Arguments[1] = integratedPixels;
	OpenCLExecutor::getExecutor().RunKernel(copyKernel);
	OpenCLExecutor::getExecutor().WaitForKernel(copyKernel);	*/
#endif

	RELEASE_MUTEX(lock);

	integratedPixels->releaseCLMemory();
	if (OnIntegrationFinished != NULL)
		OnIntegrationFinished(integratedPixels, (*CL_PixelBuffer)[from].toa);
#endif
}
