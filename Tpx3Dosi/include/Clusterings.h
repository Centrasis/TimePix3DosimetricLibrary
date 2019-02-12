#pragma once

#define _USE_MATH_DEFINES

#define MAX_TIME_DIFFERENCE_PER_CLUSTER 10 //in ns
#define MAX_LACK_OF_CLUSTERS_TIME 50 //in ns

namespace PixelClustering
{
	//Defined by CMake
#ifndef __USE_OPENCL__
	#include <KatherineTpx3.h>
	#include <vector>
	#include "TimepixTypes.h"
	#include <math.h>
	#include "ClusterTypes.h"
//returns true if clustering ended because of a MAX_LACK_OF_CLUSTERS_TIME gap and false if not
	/*
	bool NeighbouringPixels(PixelList& list, size_t start, size_t end, ClusterList& clusters, size_t & lastClusterEndPosition)
	{
		std::vector<FClusterPositionLinker> tocluster;
		std::vector<katherine_px_f_toa_tot_t*> pixelsInCluster;
		uint64_t lastToA = 0;
		bool foundNeighbour = false;

		//push pixels backwards to list, because in the loop we will pop the pixels LIFO -> first pixel to pop needs to be the first pixel in time
		for (size_t i = end; i > start; i--)
			tocluster.push_back(FClusterPositionLinker(list[i], i));
		tocluster.push_back(FClusterPositionLinker(list[start], start));

		clusters.clear();
		lastClusterEndPosition = start;

		while (!tocluster.empty())
		{
			pixelsInCluster.clear();
			pixelsInCluster.push_back(tocluster.back().px);
			uint64_t ToA = pixelsInCluster.back()->toa;
			uint64_t ToARaw = ToA;
			if (lastToA == 0)
				lastToA = ToA;
			if (ToA - lastToA > MAX_LACK_OF_CLUSTERS_TIME)
				return true;

			uint64_t highestToA = ToA;
			katherine_px_f_toa_tot_t* initial = pixelsInCluster.back();
			int pixelCount = 1;

			//get last Pixel that belongs to the last cluster in pixel list
			if (tocluster.back().positionInList > lastClusterEndPosition)
				lastClusterEndPosition = tocluster.back().positionInList;

			tocluster.pop_back();

			while (!pixelsInCluster.empty())
			{
				foundNeighbour = false;

				//needs to be checkted size_t(0) - 1  is bullshit
				if (tocluster.size() > 0)
				{
					//Remember: Pixel order is reversed
					for (long int i = tocluster.size() - 1; i >= 0; i--)
					{
						if (ToA > tocluster[i].px->toa)
						{
							if (ToA - tocluster[i].px->toa > MAX_TIME_DIFFERENCE_PER_CLUSTER)
								continue;
						}
						else
						{
							if (tocluster[i].px->toa - ToA > MAX_TIME_DIFFERENCE_PER_CLUSTER)
								continue;
						}

						//is in MAX_TIME_DIFFERENCE_PER_CLUSTER
						if (std::abs(tocluster[i].px->coord.x - pixelsInCluster.back()->coord.x) > 1)
							continue;
						if (std::abs(tocluster[i].px->coord.y - pixelsInCluster.back()->coord.y) > 1)
							continue;

						//Is neighbouring pixel
						pixelCount++;
						pixelsInCluster.push_back(tocluster[i].px);
						ToARaw += tocluster[i].px->toa;
						ToA = ToARaw / pixelCount;

						if (highestToA < tocluster[i].px->toa)
							highestToA = tocluster[i].px->toa;

						//get last pixel in pixel list that belongs to this cluster
						if (tocluster[i].positionInList > lastClusterEndPosition)
							lastClusterEndPosition = tocluster[i].positionInList;

						tocluster.erase(tocluster.begin() + i);
						foundNeighbour = true;
						break;
					}
				}

				if (!foundNeighbour)
					pixelsInCluster.pop_back();
			}
			clusters.push_back(PixelCluster(initial, pixelCount, highestToA - initial->toa));
		}

		return false;
	}*/

	bool LoG_BlobDetection(PixelList& list, size_t start, size_t end, ClusterList& clusters, size_t & lastClusterEndPosition)
	{
		//Data structure for a pixel in scale space
		typedef struct FScaleSpacePixel
		{
			katherine_coord_t coord;
			uint_fast8_t ftoa;
			double toa;
			uint_fast8_t tot;

			FScaleSpacePixel(katherine_px_f_toa_tot_t* px)
			{
				coord = px->coord;
				ftoa  = px->ftoa;
				toa   = px->toa;
				tot   = px->tot;
			}
		} FScaleSpacePixel;

		struct FHelpers
		{
			inline double GaussianKernel(uint8_t x, uint8_t y, float scale)
			{
				return (1.0 / (2.0 * M_PI * scale)) * exp(-(pow(x,2)+pow(y,2))/(2.0*scale));
			}

			FScaleSpacePixel getScaleSpacePixel(katherine_px_f_toa_tot_t* px, float scale = 1.f)
			{
				FScaleSpacePixel retPx = FScaleSpacePixel(px);

				retPx.toa *= GaussianKernel(retPx.coord.x, retPx.coord.y, scale);

				return retPx;
			}

			double getDerivativeOfPixel(FScaleSpacePixel& px)
			{
				return 0.f;// d(px.coord, x) + d(px.coord, y)
			}
		};

		return false;
	}

#else
#ifdef __USE_SYCL__
	#include <KatherineTpx3.h>
	#include <vector>
	#include "TimepixTypes.h"
	#include <math.h>
	#include "ClusterTypes.h"
	bool NeighbouringPixels(FGlobalBufferAccess<FClusterPositionLinker, SYCL_READ_WRITE>& toCluster, FGlobalBufferAccess<ClusterList, SYCL_WRITE>& clusters, FGlobalBufferAccess<size_t, SYCL_READ_WRITE>& lastClusterEndPosition)
	{
		//chg.parallel_for((size_t)((to - from) / minClusteringSementSize), cl::sycl::kernel();
		/*chg.single_task<class SimpleClusterTask>([=]()
		{

		});

		std::vector<katherine_px_f_toa_tot_t*> pixelsInCluster;
		uint64_t lastToA = 0;
		bool foundNeighbour = false;


		//push pixels backwards to list, because in the loop we will pop the pixels LIFO -> first pixel to pop needs to be the first pixel in time

		lastClusterEndPosition[0] = toCluster[0].positionInList;

		while (!tocluster.empty())
		{
			pixelsInCluster.clear();
			pixelsInCluster.push_back(tocluster.back().px);
			uint64_t ToA = pixelsInCluster.back()->toa;
			uint64_t ToARaw = ToA;
			if (lastToA == 0)
				lastToA = ToA;
			if (ToA - lastToA > MAX_LACK_OF_CLUSTERS_TIME)
				return true;

			uint64_t highestToA = ToA;
			katherine_px_f_toa_tot_t* initial = pixelsInCluster.back();
			int pixelCount = 1;

			//get last Pixel that belongs to the last cluster in pixel list
			if (tocluster.back().positionInList > lastClusterEndPosition[0])
				lastClusterEndPosition[0] = tocluster.back().positionInList;

			tocluster.pop_back();

			while (!pixelsInCluster.empty())
			{
				foundNeighbour = false;

				//needs to be checkted size_t(0) - 1  is bullshit
				if (tocluster.size() > 0)
				{
					//Remember: Pixel order is reversed
					for (long int i = tocluster.size() - 1; i >= 0; i--)
					{
						if (ToA > tocluster[i].px->toa)
						{
							if (ToA - tocluster[i].px->toa > MAX_TIME_DIFFERENCE_PER_CLUSTER)
								continue;
						}
						else
						{
							if (tocluster[i].px->toa - ToA > MAX_TIME_DIFFERENCE_PER_CLUSTER)
								continue;
						}

						//is in MAX_TIME_DIFFERENCE_PER_CLUSTER
						if (std::abs(tocluster[i].px->coord.x - pixelsInCluster.back()->coord.x) > 1)
							continue;
						if (std::abs(tocluster[i].px->coord.y - pixelsInCluster.back()->coord.y) > 1)
							continue;

						//Is neighbouring pixel
						pixelCount++;
						pixelsInCluster.push_back(tocluster[i].px);
						ToARaw += tocluster[i].px->toa;
						ToA = ToARaw / pixelCount;

						if (highestToA < tocluster[i].px->toa)
							highestToA = tocluster[i].px->toa;

						//get last pixel in pixel list that belongs to this cluster
						if (tocluster[i].positionInList > lastClusterEndPosition[0])
							lastClusterEndPosition[0] = tocluster[i].positionInList;

						tocluster.erase(tocluster.begin() + i);
						foundNeighbour = true;
						break;
					}
				}

				if (!foundNeighbour)
					pixelsInCluster.pop_back();
			}
			clusters[0].push_back(PixelCluster(initial, pixelCount, highestToA - initial->toa));
		}
		*/
		return false;
	}

#else
    #include "CL/cl.hpp"
	//#include <KatherineTpx3.h>
	#include <vector>
	//#include "TimepixTypes.h"
	//#include <math.h>
	#include "ClusterTypes.h"


	bool NeighbouringPixels(OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>* list, size_t start, size_t end, std::shared_ptr<ClusterList> clusters, size_t& lastClusterEndPosition)
	{
		std::vector<FClusterPositionLinker> tocluster;
		std::vector<katherine_px_f_toa_tot_t*> pixelsInCluster;
		uint64_t lastToA = 0;
		bool foundNeighbour = false;

		//push pixels backwards to list, because in the loop we will pop the pixels LIFO -> first pixel to pop needs to be the first pixel in time
		for (size_t i = end; i > start; i--)
			tocluster.push_back(FClusterPositionLinker(&(*list)[i], i));
		tocluster.push_back(FClusterPositionLinker(&(*list)[start], start));

		clusters->clear();
		lastClusterEndPosition = start;

		while (!tocluster.empty())
		{
			pixelsInCluster.clear();
			pixelsInCluster.push_back(tocluster.back().px);
			uint64_t ToA = pixelsInCluster.back()->toa;
			uint64_t ToARaw = ToA;
			if (lastToA == 0)
				lastToA = ToA;
			if (ToA - lastToA > MAX_LACK_OF_CLUSTERS_TIME)
				return true;

			uint64_t highestToA = ToA;
			katherine_px_f_toa_tot_t* initial = pixelsInCluster.back();
			int pixelCount = 1;

			//get last Pixel that belongs to the last cluster in pixel list
			if (tocluster.back().positionInList > lastClusterEndPosition)
				lastClusterEndPosition = tocluster.back().positionInList;

			tocluster.pop_back();

			while (!pixelsInCluster.empty())
			{
				foundNeighbour = false;

				//needs to be checkted size_t(0) - 1  is bullshit
				if (tocluster.size() > 0)
				{
					//Remember: Pixel order is reversed
					for (long int i = tocluster.size() - 1; i >= 0; i--)
					{
						if (ToA > tocluster[i].px->toa)
						{
							if (ToA - tocluster[i].px->toa > MAX_TIME_DIFFERENCE_PER_CLUSTER)
								continue;
						}
						else
						{
							if (tocluster[i].px->toa - ToA > MAX_TIME_DIFFERENCE_PER_CLUSTER)
								continue;
						}

						//is in MAX_TIME_DIFFERENCE_PER_CLUSTER
						if (std::abs(tocluster[i].px->coord.x - pixelsInCluster.back()->coord.x) > 1)
							continue;
						if (std::abs(tocluster[i].px->coord.y - pixelsInCluster.back()->coord.y) > 1)
							continue;

						//Is neighbouring pixel
						pixelCount++;
						pixelsInCluster.push_back(tocluster[i].px);
						ToARaw += tocluster[i].px->toa;
						ToA = ToARaw / pixelCount;

						if (highestToA < tocluster[i].px->toa)
							highestToA = tocluster[i].px->toa;

						//get last pixel in pixel list that belongs to this cluster
						if (tocluster[i].positionInList > lastClusterEndPosition)
							lastClusterEndPosition = tocluster[i].positionInList;

						tocluster.erase(tocluster.begin() + i);
						foundNeighbour = true;
						break;
					}
				}

				if (!foundNeighbour)
					pixelsInCluster.pop_back();
			}
			clusters->push_back(PixelCluster(initial, pixelCount, highestToA - initial->toa));
		}

		return false;
	}
#endif
#endif
}