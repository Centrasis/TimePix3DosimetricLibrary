#pragma once
#include "KatherineTpx3.h"
#include "KatherineTimepix3.h"
#include <vector>
#include "MultiplattformTypes.h"
#include <mutex>

const size_t NO_ALGORITHM_IDX = MAXINT64;

class Tpx3RadiationAngleStatistics
{
	friend class Tpx3AlgorithmStatistics;
public:
	class FAlgorithmInstance
	{
		friend Tpx3RadiationAngleStatistics;
	private:
		unsigned long long RunTime = 0;
		size_t CiclesTillTerminiation = 0;
		size_t AlgorithmRepeats = 0;
		bool Active = true;
		unsigned long long startTime;
	public:
		FAlgorithmInstance();
		~FAlgorithmInstance();
		void FinishInstance();
		void AddRepeat();
		void AddCicle();
	};

	Tpx3RadiationAngleStatistics();
	~Tpx3RadiationAngleStatistics();

	float GetProbabilityForNoResult();
	float GetProbabilityForOneRepeat();
	uint64_t GetAverageCicles();
	uint64_t GetAverageRunTime();
	unsigned long long GetTotalRunTime();

	void Reset()
	{
		Instances.clear();
	}

	FAlgorithmInstance& operator[] (size_t idx)
	{
		return Instances[idx];
	}

	size_t OpenNewAlgorithmInstance();
	size_t GetLatestActiveAlgorithmInstance();

private:
	std::vector<FAlgorithmInstance> Instances;
	MUTEXTYPE CREATEMUTEX(mutex);
};

class Tpx3AlgorithmStatistics
{
public:
	static Tpx3RadiationAngleStatistics RadiationAngleAlgorithmStatistics;
	static void Reset();
	static inline bool IsAvailable()
	{
#ifdef _DO_ALOGRITHM_STATISTICS_
		return RadiationAngleAlgorithmStatistics.Instances.size() > 0;
#else
		return false;
#endif
	}
};