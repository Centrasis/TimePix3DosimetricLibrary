#include "KatherineTimepix3.h"
#ifndef __SIMULATION__
#include <katherine/status.h>
#endif



KatherineTimepix3::KatherineTimepix3(const char* IPAddr)
{
	this->IPAddr = IPAddr;
}


KatherineTimepix3::~KatherineTimepix3()
{
	if (isConnected)
		disconnect();
}

float KatherineTimepix3::getTemperature()
{
	float ret;
#ifndef __SIMULATION__
	katherine_get_sensor_temperature(&device, &ret);
#else
	ret = 69.8f;
#endif
	//ret seems to be in Fahrenheit -> Celsius
	ret = ((ret - 32.f) * 5.f) / 9.f;
	return ret;
}

bool KatherineTimepix3::initialize()
{
	chipID = "";
#ifndef __SIMULATION__
	isConnected = katherine_device_init(&device, IPAddr.c_str()) == 0;

	char chip_id[KATHERINE_CHIP_ID_STR_SIZE];

	isConnected = isConnected && (katherine_get_chip_id(&device, chip_id) == 0);

	if (!isConnected)
		katherine_device_fini(&device);
	else
		chipID = chip_id;
#else
	isConnected = true;
	chipID = "Test_TimePix";
#endif
	return isConnected;
}

void KatherineTimepix3::disconnect()
{
#ifndef __SIMULATION__
	if (isConnected)
		katherine_device_fini(&device);
#endif
	chipID = "";
	isConnected = false;
}

std::string KatherineTimepix3::getIP()
{
	return IPAddr;
}

std::string KatherineTimepix3::getChipID()
{
	return chipID;
}

katherine_device_t * KatherineTimepix3::getReadoutDevice()
{
	return &device;
}
