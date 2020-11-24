#pragma once
#include "KatherineTpx3.h"
#include "KatherineTimepix3.h"
#include <vector>
#include "MultiplattformTypes.h"
#include <mutex>
#include <chrono>
#include <climits>

const size_t NO_ALGORITHM_IDX = ULLONG_MAX;

class Tpx3RadiationAngleStatistics
{
	friend class Tpx3AlgorithmStatistics;
public:
	class FAlgorithmInstance
	{
		friend Tpx3RadiationAngleStatistics;
	private:
		std::chrono::nanoseconds RunTime;
		size_t CiclesTillTerminiation = 0;
		size_t AlgorithmRepeats = 0;
		bool Active = true;
		std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
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
	std::chrono::nanoseconds GetAverageRunTime();
	std::chrono::nanoseconds GetTotalRunTime();

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