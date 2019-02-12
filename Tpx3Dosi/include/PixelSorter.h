#pragma once
#include <KatherineTpx3.h>
#include <MultiplattformTypes.h>
#include "TimepixTypes.h"
#include "Sorts.h"
#include <thread>
#include "SegmentDataProcessingThread.h"
#include "OpenCLTypes.h"

class PixelSorter: public SegmentDataProcessingThread
{
public:
	PixelSorter(void(*SortAlgo)(OCLTypedVariable<katherine_px_f_toa_tot_t, ASGlobal, PixelDataBufferSize>*, size_t, size_t), OCLVariable* DataBuffer, const size_t amountOfToSortPixels, const size_t BufferSize, const size_t* currentWritePos);
	~PixelSorter();

	void SetOnNewPixelsSorted(std::function<void(size_t start, size_t end)> OnSorted);

protected:
	OCLVariable* DataBuffer;

	std::function<void(size_t start, size_t end)> OnNewPixelsSorted = NULL;

	virtual void OnExecProcessing(size_t from, size_t to) override;
	void(*SortAlgo)(OCLTypedVariable<katherine_px_f_toa_tot_t, ASGlobal, PixelDataBufferSize>*, size_t, size_t) = NULL;
};

