#include "Tpx3Dosemeter.h"

#ifdef EXPORT_TPX_DOSE_API
TPX_DOSE_API Tpx3DosageMeasurement * StartDataDrivenMeasurement(TimePix3DosemeterConfig* config)
{
#ifdef __SIMULATION__
	Tpx3DosageMeasurement* con = new Tpx3DosageMeasurement(std::string(config->basePath));
#else
	Tpx3DosageMeasurement* con = new Tpx3DosageMeasurement();
#endif
	con->setFilterSetup(std::string(config->FilterSetupPath), config->DetektorPixels[0], config->DetektorPixels[1]);

	if (!con->configureTimepix(config->TimePixConfigFilePath))
	{
#ifdef __SIMULATION__
		if (!con->searchTimepix())
#else
		if (!con->searchTimepix(config->TpxIPBounds[0], config->TpxIPBounds[1]))
#endif
		{
			throw Tpx3Exception("No timepix present!");
			delete con;
			return NULL;
		}
	}

	if (!con->doDataDrivenMode())
	{
		throw Tpx3Exception("Could not start measurement!");
		delete con;
		return NULL;
	}

	return con;
}

TPX_DOSE_API uint64_t GetCurrentElapsedTime_ms(Tpx3DosageMeasurement * con)
{
	return con->getElapsedProcessedTime();
}

TPX_DOSE_API cv::Mat* GetCurrentFrameWithShadows(Tpx3DosageMeasurement * con)
{
	return &con->getCurrentFrameWithFoundShadows();
}

TPX_DOSE_API cv::Mat * GetCurrentFrame(Tpx3DosageMeasurement * con)
{
	return &con->getCurrentFrame();
}

TPX_DOSE_API FVector3D GetCurrentRadiationAngles(Tpx3DosageMeasurement * con)
{
	return con->getRadiationDirection() * (180.f/M_PI);
}
#endif