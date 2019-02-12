#pragma once
#include <KatherineTpx3.h>
#ifdef __SIMULATION__
  #include "TpxSimulation.h"
#endif
#ifdef __USE_OPENCL__
  #include "OpenCLTypes.h"
  #include "OpenCLExecutor.h"
#endif
#include "SegmentDataProcessingThread.h"
#include <functional>
#include "opencv2/opencv.hpp"
#include "MultiplattformTypes.h"
#include <vector>

class PixelList;
class RadiationAngleReconstructor;

//#define USE_BLUR

typedef std::function<void(
#ifdef __USE_OPENCL__
	std::shared_ptr<OCLMemoryVariable<cl::Image2D>> integrationResult,
#endif
	uint64_t timeStamp
	)> FOnIntegrationFinished;

class PixelIntegrator :
	public SegmentDataProcessingThread
{
public:
	PixelIntegrator(OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>* pixellist, uint64_t integrationTime, const size_t BufferSize);
	virtual ~PixelIntegrator();
	void SetOnIntegrationFinished(FOnIntegrationFinished onFinished);
	void SetFrameBuffer(cv::Mat& img);
	void UpdateFrame(bool shouldReadBlocking = false);
	std::shared_ptr<OCLMemoryVariable<cl::Image2D>> getCLIntegrationBuffer() { return CL_IntegratedPixels.back(); };

	uint64_t getElapsed_ms();
	uint64_t getElapsed_ns();
	float getHitsPerSecond();
	//a count of 0 means that no minimal event count is needed
	void setMinimalEventsPerFrame(uint64_t count);

protected:
	virtual bool EvaluateSegmentReady(size_t& amount) override;
	virtual void OnExecProcessing(size_t from, size_t to) override;
	//PixelList* pixelList;

#ifdef __USE_OPENCL__
	OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>* CL_PixelBuffer;
	OCLTypedVariable<size_t, ASPrivate> CL_PixelBufferLen = OCLTypedVariable<size_t, ASPrivate>(PixelDataBufferSize, "", true, ATRead);
	OCLTypedVariable<size_t, ASPrivate> CL_BufferLowAddress;
	OCLTypedVariable<size_t, ASPrivate> CL_BufferHighAddress;
	OCLTypedVariable<char> CL_ThreadCount;
	OCLTypedVariable<int> CL_Iteration;
	std::vector<std::shared_ptr<OCLMemoryVariable<cl::Image2D>>> CL_IntegratedPixels;
	cv::Mat* img;
	FOCLKernel PixelIntegrateKernel = loadOCLKernel(PixelIntegrator, "main_kernel");
#endif

private:
	//integration Time in ns
	uint64_t integrationTime;
	uint64_t minimalEventsPerFrame = 0;
	uint64_t beginTime = 0;
	cl_uint4 backgroundColor;
	bool bIsFirstCall = true;
	uint64_t elapsedTime_ns = 0;
	float	hitsPerSecond = 0.f;
	FOnIntegrationFinished OnIntegrationFinished = NULL;
	int img_width, img_height;
	void* imgData;
	unsigned int maxImageBuffers = 2;
	MUTEXTYPE lock;
#ifdef USE_BLUR
	float sigma = 2.f;
	OCLDynamicTypedBuffer<float> OCLMask;
	OCLTypedVariable<int, ASPrivate> OCLMaskSize;
	FOCLKernel gaussianKernel = loadOCLKernel(PixelClustering, "gaussianBlur");
	//FOCLKernel copyKernel = loadOCLKernel("PixelClusterer", "copyImg2Img");
	//OCLMemoryVariable<cl::Image2D> blurredImg = OCLMemoryVariable<cl::Image2D>(cl::Image2D(OpenCLExecutor::getExecutor().getContext(), EOCLAccessTypes::ATReadWrite, cl::ImageFormat(CL_R, CL_UNSIGNED_INT8), 256, 256), "", true, ATReadWrite);
#endif
};
