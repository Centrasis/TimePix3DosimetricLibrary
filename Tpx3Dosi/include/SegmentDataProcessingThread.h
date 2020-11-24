#pragma once
#include <thread>
#include "KatherineTpx3.h"


class SegmentDataProcessingThread
{
public:
	SegmentDataProcessingThread(const size_t BufferSize, const size_t* maxProcessingPos, size_t minAmount, size_t maxAmount);
	virtual ~SegmentDataProcessingThread();

	virtual void EndProcessing(bool ignoreZombies = false);
	void StartProcessing();
	virtual bool IsProcessing();

	//Gets upper constraint from other thread
	void SetUpperLimitingThread(SegmentDataProcessingThread* other);
	//Returns the start of the current processing window
	inline size_t GetCurrentProcessingPos() { return currentProcessingPos; };
	inline size_t GetUpperProcessingConstraint() { return (UpperProcessingConstraint != NULL) ? *UpperProcessingConstraint : currentProcessingPos; };
	inline size_t GetMaxProcessingAmount() { return maximalProcessingAmount; };
	inline size_t GetMinProcessingAmount() { return minimalProcessingAmount; };


protected:
	size_t MaxBufferSize = 0;
	size_t processingStepWidth = 1;
	/** override to redefine when segement is ready and return the segment length */
	virtual bool EvaluateSegmentReady(size_t& amount);
	virtual void OnExecProcessing(size_t from, size_t to) = 0;
	//reverts the processing pos to the last pos (according to processingStepWidth) to redo that last processing
	void RevertProcessingStep();

protected:
	std::thread* processingThread = NULL;
	const size_t* UpperProcessingConstraint = NULL;
	size_t currentProcessingPos = 0;
	size_t minimalProcessingAmount = 0;
	//maximal amount of data to be processed at a time; a value of 0 means there is no upper limit besides the max buffer size
	size_t maximalProcessingAmount = 0;
	bool bShouldProcess = false;
	bool bIsProcessing = false;
	void ExecThreadEvaluation();

private:
	bool endedThread = false;
	bool autoCleanUpObject = false;
};
