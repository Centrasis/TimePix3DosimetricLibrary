#pragma once
#include "KatherineTpx3.h"
#include <functional>
#include "SYCLTypes.h"
#include "TimepixTypes.h"
#include "OpenCLTypes.h"

typedef struct FClusterPositionLinker
{
#ifdef __USE_OPENCL__
	katherine_px_f_toa_tot_t* px;
#else
	katherine_px_f_toa_tot_t* px;
#endif
	size_t positionInList;
#ifdef __USE_OPENCL__
	FClusterPositionLinker(katherine_px_f_toa_tot_t* px, size_t pos)
#else
	FClusterPositionLinker(katherine_px_f_toa_tot_t* px, size_t pos)
#endif
	{
		this->px = px;
		positionInList = pos;
	}
} FClusterPositionLinker;

#ifdef __USE_OPENCL__
	#ifdef __USE_SYCL__
	typedef bool (*FClusteringMethod)(FGlobalBufferAccess<FClusterPositionLinker, SYCL_READ_WRITE>& toCluster, FGlobalBufferAccess<ClusterList, SYCL_WRITE>& clusters, FGlobalBufferAccess<size_t, SYCL_READ_WRITE>& lastClusterEndPosition);
	#else
	typedef bool(*FClusteringMethod)(OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>* toCluster, size_t start, size_t end, std::shared_ptr<ClusterList> clusters, size_t& lastClusterEndPosition);//(cl::Buffer& toCluster, cl::Buffer& clusters, cl::Buffer& lastClusterEndPosition)
	#endif
//typedef void* FClusteringMethod;//Dummy implement
#else
typedef bool(*FClusteringMethod)(PixelList& list, size_t start, size_t end, ClusterList& clusters, size_t & lastClusterEndPosition);
#endif