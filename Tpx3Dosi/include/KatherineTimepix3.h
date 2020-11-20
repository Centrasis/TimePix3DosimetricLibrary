#pragma once
#include <KatherineTpx3.h>
#include <string>

#ifndef __SIMULATION__
#include <katherine/device.h>
#else
#include <TpxSimulation.h>
#endif

class KatherineTimepix3
{
public:
	KatherineTimepix3(const char* IPAddr);
	~KatherineTimepix3();

	bool initialize();

	void disconnect();

	std::string getIP();
	std::string getChipID();
	katherine_device_t* getReadoutDevice();

	float getTemperature();

	explicit operator bool()
	{
		return (this->getChipID().length() > 0);
	}

	friend bool operator== (KatherineTimepix3& lhs, KatherineTimepix3& rhs)
	{
		return lhs.getIP().compare(rhs.getIP()) == 0;
	}

protected:
	std::string IPAddr;
	std::string chipID;
	bool isConnected;
	katherine_device_t device;
};