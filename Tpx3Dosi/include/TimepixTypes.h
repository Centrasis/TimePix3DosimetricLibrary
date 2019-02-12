#pragma once
#ifndef __SIMULATION__
#include <katherine/acquisition.h>
#else
#include <TpxSimulation.h>
#endif
#include <vector>

//From Juergen timepix3analysis.pas
#define TPX_ROLLOVER_VALUE 1073741824

class PixelList
{
public:
	PixelList(size_t bufferSize)
	{
		this->bufferSize = bufferSize;
		pixels = (katherine_px_f_toa_tot_t**)malloc(bufferSize * sizeof(katherine_px_f_toa_tot_t**));
	};

	~PixelList()
	{
		free(pixels);
	};

	/** @return length of this list */
	size_t Size()
	{
		return bufferSize;
	}

	/** @return size of underlying datasructure multiplyed by the length of this list */
	size_t handledDataSize()
	{
		return bufferSize * sizeof(katherine_px_f_toa_tot_t);
	}

	katherine_px_f_toa_tot_t** getPixelSegmentBetweenTimes(uint64_t begin_ns, uint64_t end_ns, size_t& listLength)
	{
		listLength = 0;
		katherine_px_f_toa_tot_t** retVal = NULL;

		if (begin_ns > end_ns)
		{
			uint64_t temp = begin_ns;
			begin_ns = end_ns;
			end_ns = temp;
		}

		for (size_t i = highestAccess; i > 0; i--)
		{
			if (pixels[i]->toa <= end_ns && pixels[i]->toa >= begin_ns)
			{
				listLength++;
			}
			else
			{
				if (listLength > 0 && i < bufferSize - 2)
				{
					retVal = &pixels[i+1];
				}
			}
		}

		if (highestAccess > 0)
		{
			if (pixels[0]->toa <= end_ns && pixels[0]->toa >= begin_ns)
			{
				listLength++;
			}
			else
			{
				if (listLength > 0 && 0 < bufferSize - 2)
				{
					retVal = &pixels[1];
				}
			}
		}


		if (retVal == NULL && listLength > 0)
			retVal = &pixels[0];

		return retVal;
	}

	size_t getPixelIdxByToA(uint64_t ToA, size_t offset = 0)
	{
		for (size_t i = offset; i < bufferSize; i++)
		{
			if (pixels[i]->toa > ToA)
			{
				return i-1;
			}
		}
		return offset;
	}

	inline katherine_px_f_toa_tot_t*& operator[] (size_t x) 
	{
		if (x < bufferSize)
		{
			if (x > highestAccess)
				highestAccess = x;
			return *(pixels + x);
		}
		else
		{
			if ((x - bufferSize) > highestAccess)
				highestAccess = (x - bufferSize);
			return *(pixels + (x - bufferSize));
		}
	}

	inline katherine_px_f_toa_tot_t*& getLast()
	{
		return pixels[bufferSize - 1];
	}

private:
	size_t bufferSize;
	size_t highestAccess = 0;
	katherine_px_f_toa_tot_t** pixels;
};

inline bool operator> (katherine_px_f_toa_tot_t & lhs, katherine_px_f_toa_tot_t & rhs)
{
	return lhs.toa > rhs.toa;
}

inline bool operator< (katherine_px_f_toa_tot_t & lhs, katherine_px_f_toa_tot_t & rhs)
{
	return lhs.toa < rhs.toa;
}

inline bool operator== (katherine_px_f_toa_tot_t & lhs, katherine_px_f_toa_tot_t & rhs)
{
	return lhs.toa == rhs.toa;
}

inline bool operator>= (katherine_px_f_toa_tot_t & lhs, katherine_px_f_toa_tot_t & rhs)
{
	return ((lhs == rhs) || (lhs > rhs));
}

inline bool operator<= (katherine_px_f_toa_tot_t & lhs, katherine_px_f_toa_tot_t & rhs)
{
	return ((lhs == rhs) || (lhs < rhs));
}


typedef struct PixelCluster
{
	katherine_px_f_toa_tot_t* initialHit;
	int pixelCount;
	uint64_t duration;

	PixelCluster(katherine_px_f_toa_tot_t* initialHit, int pixelCount, uint64_t duration)
	{
		this->initialHit = initialHit;
		this->duration = duration;
		this->pixelCount = pixelCount;
	}
} PixelCluster;

inline bool operator== (PixelCluster & lhs, PixelCluster & rhs)
{
	return *lhs.initialHit == *rhs.initialHit;
}

typedef std::vector<PixelCluster> ClusterList;