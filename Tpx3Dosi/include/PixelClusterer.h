#pragma once

#include <KatherineTpx3.h>
#include <functional>
#include "TimepixTypes.h"
#include "SegmentDataProcessingThread.h"
#include "ClusterTypes.h"
#include "OpenCLTypes.h"
#include "opencv2/core.hpp"

class SimpleClusterTask;
#ifndef __USE_SYCL__
//why?
typedef bool(*FClusteringMethod)(OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>* toCluster, size_t start, size_t end, std::shared_ptr<ClusterList> clusters, size_t& lastClusterEndPosition);
#endif

class PixelClusterer: public SegmentDataProcessingThread
{
public:
	PixelClusterer(FClusteringMethod ClusteringAlgo, OCLVariable* pixellist, const size_t amountOfMinToClusterPixels, const size_t BufferSize, 
		std::function<void (std::shared_ptr<ClusterList> clusters)> OnClusteringFinish, float sigma = 3.f);
	~PixelClusterer();
	void setWorkImage(cv::Mat& img);
	void addIntegrationResult(std::shared_ptr<OCLMemoryVariable<cl::Image2D>> integrationResult);
	void UpdateFrame();

 protected:
	virtual void OnExecProcessing(size_t from, size_t to) override;
	virtual bool EvaluateSegmentReady(size_t & amount) override;

private:
	FClusteringMethod ClusteringAlgo;
	std::function<void(std::shared_ptr<ClusterList> clusters)> OnClusteringFinish;
	OCLVariable* pixels;
	std::shared_ptr<ClusterList> clusters;
	size_t minClusteringSementSize = 0;
	size_t lastClusterEndPosition = 0;
	cv::Mat* WorkImage = NULL;
	OCLDynamicTypedBuffer<float>** OCLMask;
	OCLTypedVariable<int, ASPrivate> OCLMaskSize;
	float sigma;
	FOCLKernel* gaussianKernel;
	std::shared_ptr<OCLMemoryVariable<cl::Image2D>>* blurredImg;
	std::vector<std::shared_ptr<OCLMemoryVariable<cl::Image2D>>> integrationResults;
	MUTEXTYPE lock;
	float sigmas[2] = { 2.f, 4.f };
};
