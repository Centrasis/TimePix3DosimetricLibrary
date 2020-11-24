#include "PixelSorter.h"

PixelSorter::PixelSorter(void(*SortAlgo)(OCLTypedVariable<katherine_px_f_toa_tot_t, ASGlobal, PixelDataBufferSize>*, size_t, size_t), OCLVariable* DataBuffer, const size_t amountOfToSortPixels, const size_t BufferSize, const size_t* currentWritePos)
	: SegmentDataProcessingThread::SegmentDataProcessingThread(BufferSize, currentWritePos, amountOfToSortPixels, amountOfToSortPixels)
{
	this->DataBuffer = DataBuffer;
	//this->pixelList = list;

	this->SortAlgo = SortAlgo;
}

PixelSorter::~PixelSorter()
{
}

void PixelSorter::SetOnNewPixelsSorted(std::function<void(size_t start, size_t end)> OnSorted)
{
	OnNewPixelsSorted = OnSorted;
}


void PixelSorter::OnExecProcessing(size_t from, size_t to)
{
	SortAlgo((OCLTypedVariable<katherine_px_f_toa_tot_t, ASGlobal, PixelDataBufferSize>*)DataBuffer, from, to);

	((OCLTypedRingBuffer<katherine_px_f_toa_tot_t, PixelDataBufferSize>*)DataBuffer)->setReadEndPosForCLDevice(to);

	if (OnNewPixelsSorted != NULL)
		OnNewPixelsSorted(from, to);
}
