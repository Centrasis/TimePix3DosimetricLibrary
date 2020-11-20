#pragma once
#include "OpenCLTypes.h"
#ifdef __USE_OPENGL__
  #include "OpenCLGLExecutorAdapter.h"
#else
  #include "OpenCLExecutor.h"
#endif  
#include "opencv2/core.hpp"

class TimepixDataCLProc :
	public 
#ifdef __USE_OPENGL__
			OpenCLGLExecutorAdapter
#else
			OpenCLExecutor
#endif
{
protected:
	TimepixDataCLProc();
	virtual ~TimepixDataCLProc();
	FOCLKernel shadowReconstructor;
public:
	static TimepixDataCLProc& getExecutor();
	void LaunchShadowReconstructor();
	void LaunchPixelIntegrator(uint64_t integrationDuration);
};
