#include "TimepixDataCLProc.h"

TimepixDataCLProc::TimepixDataCLProc() :
#ifdef __USE_OPENGL__
	OpenCLGLExecutorAdapter()
#else
	OpenCLExecutor()
#endif
{
//	shadowReconstructor = loadOCLKernel(RadiationAngleReconstructor, "main_kernel");
}

TimepixDataCLProc::~TimepixDataCLProc()
{
}

TimepixDataCLProc & TimepixDataCLProc::getExecutor()
{
	if (!IS_MUTEX_VALID(CL_LOCK))
		CREATEMUTEX(CL_LOCK);

	ACQUIRE_MUTEX(CL_LOCK);
	if (internalExec == NULL)
	{
		std::printf("Init new TimePix Executor\n");
		internalExec = new TimepixDataCLProc();
		if(internalExec->InitPlatform())
			std::printf("OpenCL ready!\n");
		else
			std::printf("OpenCL failture!\n");
	}
	RELEASE_MUTEX(CL_LOCK);

	return *((TimepixDataCLProc*)internalExec);
}

void TimepixDataCLProc::LaunchShadowReconstructor()
{
	/*if (!runsKernel(shadowReconstructor))
	{
		InitKernel(shadowReconstructor);

#ifdef __USE_OPENGL__
		auto ImgBound = addCLGLImageToKernel(shadowReconstructor, img);
		//createCLGLRenderBuffer(256, 256);

		//WriteTextureData(ImgBound, &img);

		shadowReconstructor.Arguments.push_back((OCLVariable*) new OCLMemoryVariable<cl::ImageGL>(&ImgBound.CLHandle));
//		shadowReconstructor.Arguments.push_back((OCLVariable*) new OCLMemoryVariable<cl::Image2D>(&ImgBound.CLHandle));
#else

#endif

		RunInitializedKernel(shadowReconstructor, false);
	}*/
}

void TimepixDataCLProc::LaunchPixelIntegrator(uint64_t integrationDuration)
{

}