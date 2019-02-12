#pragma once
#include "KatherineTpx3.h"
#include "KatherineTimepix3.h"
#include "Tpx3DosageMeasurement.h"

#ifdef EXPORT_TPX_DOSE_API
#ifdef WIN32
  #define TPX_DOSE_API extern "C" __declspec(dllexport)
#else
  #define TPX_DOSE_API extern "C" 
#endif

struct Tpx3Exception : public std::exception
{
	explicit Tpx3Exception(std::string _Message) noexcept
		: std::exception(_Message.c_str())
	{

	}

	const char * what() const throw () {
		return "TimePix3 Dosemeter Exception";
	}
};

typedef struct TimePix3DosemeterConfig
{
#ifdef __SIMULATION__
	char* basePath;
#else
	char TpxIPBounds[2];
#endif
	char* FilterSetupPath;
	char* TimePixConfigFilePath;
	unsigned int DetektorPixels[2];

	TimePix3DosemeterConfig(
#ifdef __SIMULATION__
		char* basePath,
#endif
		char* FilterSetupPath,
		char* TimePixConfigFilePath,
		unsigned int DetektorPixels[2]
	)
	{
#ifdef __SIMULATION__
		this->basePath = basePath;
#endif
		this->FilterSetupPath = FilterSetupPath;
		this->TimePixConfigFilePath = TimePixConfigFilePath;
		this->DetektorPixels[0] = DetektorPixels[0];
		this->DetektorPixels[1] = DetektorPixels[1];
	}
};

TPX_DOSE_API Tpx3DosageMeasurement* StartDataDrivenMeasurement(TimePix3DosemeterConfig* config);
TPX_DOSE_API uint64_t GetCurrentElapsedTime_ms(Tpx3DosageMeasurement* con);
TPX_DOSE_API cv::Mat* GetCurrentFrameWithShadows(Tpx3DosageMeasurement* con);
TPX_DOSE_API cv::Mat* GetCurrentFrame(Tpx3DosageMeasurement* con);
TPX_DOSE_API FVector3D GetCurrentRadiationAngles(Tpx3DosageMeasurement* con);
#endif