#include "Tpx3AlgorithmStatistics.h"
#include "RadiationAngleReconstructor.h"

Tpx3RadiationAngleStatistics Tpx3AlgorithmStatistics::RadiationAngleAlgorithmStatistics;

Tpx3RadiationAngleStatistics::FAlgorithmInstance::FAlgorithmInstance()
{
	startTime = GetTickCount64();
}

Tpx3RadiationAngleStatistics::FAlgorithmInstance::~FAlgorithmInstance()
{
	
}

void Tpx3RadiationAngleStatistics::FAlgorithmInstance::FinishInstance()
{
	Active = false;
	RunTime = GetTickCount64() - startTime;
}

void Tpx3RadiationAngleStatistics::FAlgorithmInstance::AddRepeat()
{
	if (!Active)
		return;
	AlgorithmRepeats++;
	CiclesTillTerminiation = 0;
}

void Tpx3RadiationAngleStatistics::FAlgorithmInstance::AddCicle()
{
	if (!Active)
		return;
	CiclesTillTerminiation++;
}

Tpx3RadiationAngleStatistics::Tpx3RadiationAngleStatistics()
{	
}

Tpx3RadiationAngleStatistics::~Tpx3RadiationAngleStatistics()
{
	DESTROYMUTEX(mutex);
}

float Tpx3RadiationAngleStatistics::GetProbabilityForNoResult()
{
	size_t AlgoExecutionsWithNoResult = 0;
	for (auto i : Instances)
		if (i.AlgorithmRepeats > RadiationAngleReconstructor::RepeatsCount)
			AlgoExecutionsWithNoResult++;
	return (float)AlgoExecutionsWithNoResult / (float)Instances.size();
}

float Tpx3RadiationAngleStatistics::GetProbabilityForOneRepeat()
{
	size_t repeatedAlgoExecutions = 0;
	for (auto i : Instances)
		if (i.AlgorithmRepeats > 0)
			repeatedAlgoExecutions++;
	return (float)repeatedAlgoExecutions/(float)Instances.size();
}

uint64_t Tpx3RadiationAngleStatistics::GetAverageCicles()
{
	uint64_t TotalCicles = 0;
	for (auto i : Instances)
		TotalCicles += i.CiclesTillTerminiation;
	return TotalCicles/Instances.size();
}

uint64_t Tpx3RadiationAngleStatistics::GetAverageRunTime()
{
	unsigned long long TotalTime = GetTotalRunTime();

	return TotalTime/Instances.size();
}

unsigned long long Tpx3RadiationAngleStatistics::GetTotalRunTime()
{
	unsigned long long TotalTime = 0;
	for (auto i : Instances)
		TotalTime += i.RunTime;
	return TotalTime;
}

size_t Tpx3RadiationAngleStatistics::OpenNewAlgorithmInstance()
{
	ACQUIRE_MUTEX(mutex);
	Instances.push_back(FAlgorithmInstance());
	size_t ret = Instances.size() - 1;
	RELEASE_MUTEX(mutex);
	return ret;
}

size_t Tpx3RadiationAngleStatistics::GetLatestActiveAlgorithmInstance()
{
	if (Instances.size() == 0)
		return NO_ALGORITHM_IDX;

	for(size_t i = Instances.size() - 1; i > 0; i--)
		if(Instances[i].Active)
			return i;

	if (Instances[0].Active)
		return 0;
	return NO_ALGORITHM_IDX;
}


void Tpx3AlgorithmStatistics::Reset()
{
	RadiationAngleAlgorithmStatistics.Reset();
}