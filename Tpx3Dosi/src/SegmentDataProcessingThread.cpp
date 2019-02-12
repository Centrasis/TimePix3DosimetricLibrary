#include "SegmentDataProcessingThread.h"
#ifdef WIN32
#include <Windows.h>
#endif

SegmentDataProcessingThread::SegmentDataProcessingThread(const size_t BufferSize, const size_t * maxProcessingPos, size_t minAmount, size_t maxAmount)
{
	MaxBufferSize = BufferSize;
	UpperProcessingConstraint = maxProcessingPos;
	minimalProcessingAmount = minAmount;
	maximalProcessingAmount = maxAmount;
}

SegmentDataProcessingThread::~SegmentDataProcessingThread()
{
}

void SegmentDataProcessingThread::EndProcessing(bool ignoreZombies)
{
	bShouldProcess = false;

	if (!ignoreZombies)
	{
	#ifdef WIN32
		TerminateThread(processingThread->native_handle(), 0);
		processingThread->detach();
		while (!endedThread) { Sleep(50); }
	#else
		processingThread->join();
	#endif
		delete processingThread;
	}
	else
	{
		autoCleanUpObject = true;
	}

	processingThread = NULL;
}

void SegmentDataProcessingThread::StartProcessing()
{
	bShouldProcess = true;
	if (!processingThread)
		processingThread = new std::thread(&SegmentDataProcessingThread::ExecThreadEvaluation, this);
}

bool SegmentDataProcessingThread::IsProcessing()
{
	return bIsProcessing;
}

void SegmentDataProcessingThread::SetUpperLimitingThread(SegmentDataProcessingThread * other)
{
	if (other == NULL)
	{
		UpperProcessingConstraint = NULL;
	}
	else
		UpperProcessingConstraint = &other->currentProcessingPos;
}

bool SegmentDataProcessingThread::EvaluateSegmentReady(size_t& amount)
{
	if (UpperProcessingConstraint == NULL)
		return false;

	size_t pos = 0;
	long long diff = 0;
	pos = *UpperProcessingConstraint;
	if (pos > 0)
		pos -= 1;

	diff = pos - currentProcessingPos;

	//means pos is ahead of currentPos and no wrapping has happend yet
	if (diff >= 0)
	{
		if ((size_t)diff >= minimalProcessingAmount)
		{
			amount = (size_t)diff;
			if (maximalProcessingAmount > 0 && amount > maximalProcessingAmount)
				amount = maximalProcessingAmount;

			return true;
		}
	}
	else
	{
		//else there happend wrapping of the upper constraint due to the ring buffer property
		//calc amount with wrapping
		amount = MaxBufferSize - currentProcessingPos;
		amount += pos;
		if (amount > minimalProcessingAmount)
		{
			if (maximalProcessingAmount > 0 && amount > maximalProcessingAmount)
				amount = maximalProcessingAmount;

			return true;
		}
	}

	return false;
}

void SegmentDataProcessingThread::RevertProcessingStep()
{
	if (processingStepWidth <= currentProcessingPos)
		currentProcessingPos -= processingStepWidth;
	else
		currentProcessingPos = MaxBufferSize - (processingStepWidth - currentProcessingPos);
}

void SegmentDataProcessingThread::ExecThreadEvaluation()
{	
	size_t amount = 0;
	currentProcessingPos = 0;

	if (bIsProcessing)
		bIsProcessing = false;

	while (bShouldProcess)
	{
		
		if (EvaluateSegmentReady(amount))
		{
			if (!bIsProcessing)
				bIsProcessing = true;

			OnExecProcessing(currentProcessingPos, currentProcessingPos + amount);
			currentProcessingPos += processingStepWidth;
		}
		else
		{
			if (bIsProcessing)
				bIsProcessing = false;
		}

		//Wrap currentProcessingSize
		if (currentProcessingPos >= MaxBufferSize)
			currentProcessingPos = currentProcessingPos - MaxBufferSize;
	}
	endedThread = true;
	if (autoCleanUpObject)
		delete this;
}
