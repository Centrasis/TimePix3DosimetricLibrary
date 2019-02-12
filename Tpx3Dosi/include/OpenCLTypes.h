#pragma once

#include "KatherineTpx3.h"
#include <vector>
#include <string>
#include <CL/cl2.hpp>

#ifndef __USE_COMPILETIMERESSOURCES__
#include <filesystem>
#include <Windows.h>
#endif
#ifdef  __USE_COMPILETIMERESSOURCES__
#include "OCLRes.h"
#include <algorithm>
#define CALL_OCL_METHOD(_name_) get_##_name_()
#endif
#include <fstream>
#include <sstream>
#include <assert.h>
#include "MultiplattformTypes.h"
#include <regex>
#include <exception>
#include <memory>

namespace cl
{
	#define CL_READ CL_MEM_READ_ONLY
	#define CL_WRITE CL_MEM_WRITE_ONLY
	#define CL_READ_WRITE CL_MEM_READ_WRITE

	template<typename T>
	cl::Buffer& CreateBuffer(cl::Context& context, int length, int access_mode)
	{
		return *(new cl::Buffer(context, access_mode, length * sizeof(T)));
	}
}

struct OCLException : public std::exception
{
	explicit OCLException(std::string _Message) noexcept
		: std::exception(_Message.c_str())
	{

	}

	const char * what() const throw () {
		return "OpenCL Exception";
	}
};

enum EOCLAccessTypes
{
	ATRead = CL_MEM_READ_ONLY,
	ATWrite = CL_MEM_WRITE_ONLY,
	ATReadWrite = CL_MEM_READ_WRITE,
	ATReadCopy = CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR
};

enum EOCLArgumentScope
{
	ASGlobal = 0,
	ASLocal = 1,
	ASPrivate = 2
};

enum EOCLBufferType
{
	BTNative,
	BTCLMem,
	BTCLImage,
	BTGLImage
};

//Base Type for usage in kernel only!
class OCLVariable
{
public:
	OCLVariable(std::string name = "", bool bIsBlocking = true,	EOCLAccessTypes accessType = ATReadWrite)
	{
		this->name = name;
		this->bIsBlocking = bIsBlocking;
		this->accessType = accessType;
		CREATEMUTEX(CL_LOCK);
		CREATEMUTEX(HOST_LOCK);
		refCount = 0;
	};

	virtual ~OCLVariable() { /*if(IS_MUTEX_VALID(CL_LOCK)) DESTROYMUTEX(CL_LOCK);*/ };

	inline std::string getName() { return name; };
	inline bool getIsBlocking() { return bIsBlocking; };
	inline EOCLAccessTypes getAccessType() { return accessType; };

	virtual void* getValue() = 0;
	virtual void* getHostPointer() { return getValue(); };
	virtual void  setValue(void* val) = 0;
	virtual void setHostPointer(void* val) { setValue(val); };
	virtual size_t getTypeSize() = 0;
	virtual size_t getSize() = 0;
	virtual size_t getAvailableData() { return getSize(); };
	virtual bool needsCLBuffer() { return true; };
	virtual EOCLBufferType getBufferType() = 0;
	virtual size_t getDataOffset() { return 0; };
	virtual cl::Memory* getCLMemoryObject(cl::Context* context) = 0;
	void setVariableChanged(bool val = true)
	{
		bisUploaded = !val;
	}
	virtual cl_int uploadBuffer(cl::CommandQueue* queue)
	{
		if (bisUploaded || getCLMemoryObject(NULL) == NULL || getAccessType() == ATWrite)
			return CL_SUCCESS;

		size_t offset = getDataOffset();
		size_t actualDataSize = getAvailableData();
		if (actualDataSize == 0)
		{
			return CL_SUCCESS;
		}
		void* ptr = getValue();

		if (offset + actualDataSize > getSize())
			throw OCLException("trying to read from undefined buffer position!");

		cl_int errCode = queue->enqueueWriteBuffer(*(cl::Buffer*)getCLMemoryObject(NULL), getIsBlocking() ? CL_TRUE : CL_FALSE, offset, actualDataSize, ptr);
		if(errCode == CL_SUCCESS)
			bisUploaded = true;

		return errCode;
	}

	virtual cl_int downloadBuffer(cl::CommandQueue* queue)
	{
		if (getCLMemoryObject(NULL) == NULL)
			return CL_SUCCESS;

		if (getAccessType() == EOCLAccessTypes::ATWrite || getAccessType() == EOCLAccessTypes::ATReadWrite)
			return queue->enqueueReadBuffer(*(cl::Buffer*)getCLMemoryObject(NULL), getIsBlocking() ? CL_TRUE : CL_FALSE, 0, getSize(), getValue());

		return CL_SUCCESS;
	}

	/** only implemented for cl memory only objects */
	virtual cl_int initWithValue(cl::CommandQueue* queue, void* data, size_t size)
	{
		return CL_SUCCESS;
	}

	/*variable needs to be acquired on a relative synchronized call 
	inline void acquireHostVariable()
	{
		ACQUIRE_MUTEX(HOST_LOCK);
		refCount++;
		RELEASE_MUTEX(HOST_LOCK);
	}*/

	/** releases Variable form Host. if this OCLVariable is no longer referenced by any other instance it will be deleted 
	inline void releaseHostVariable()
	{
		ACQUIRE_MUTEX(HOST_LOCK);
		if(refCount != 0)
			refCount--;

		if (refCount == 0)
		{
			RELEASE_MUTEX(HOST_LOCK);
			delete this;
			return;
		}
		RELEASE_MUTEX(HOST_LOCK);
	}*/

	/* allows variable to be freed if refCount is zero 
	inline void Free()
	{
		ACQUIRE_MUTEX(HOST_LOCK);
		if (refCount == 0)
		{
			RELEASE_MUTEX(HOST_LOCK);
			delete this;
			return;
		}
		RELEASE_MUTEX(HOST_LOCK);
	}*/

	inline void acquireCLMemory()
	{
		if (getCLMemoryObject(NULL) != NULL)
		{
			ACQUIRE_MUTEX(CL_LOCK);
		}
	}

	inline void releaseCLMemory()
	{
		RELEASE_MUTEX(CL_LOCK);
	}

protected:
	std::string name;
	bool bisUploaded = false;
	bool bIsBlocking;
	EOCLAccessTypes accessType;
	unsigned int refCount = 0;
	MUTEXTYPE CL_LOCK;
	MUTEXTYPE HOST_LOCK;
};
template<typename T, EOCLArgumentScope TScope = EOCLArgumentScope::ASGlobal>
class OCLDynamicTypedBuffer : public OCLVariable
{
protected:
	T* value = NULL;
	size_t currentSize = 0;
	cl::Buffer* memoryBuffer = NULL;
public:
	OCLDynamicTypedBuffer(T* val = NULL, size_t size = 0, std::string name = "", bool bIsBlocking = true, EOCLAccessTypes accessType = EOCLAccessTypes::ATReadWrite) : OCLVariable(name, bIsBlocking, accessType)
	{
		if (size != 0)
		{
			value = (T*)malloc(size * sizeof(T));
			currentSize = size;
		}
		else
		{
			return;
		}
		if (val == NULL)
			return;

		for (int i = 0; i < size; i++)
		{
			value[i] = val[i];
		}
	}

	virtual ~OCLDynamicTypedBuffer() override
	{
		if(currentSize > 0)
			delete[] value;
		OCLVariable::~OCLVariable();
	}

	OCLDynamicTypedBuffer(const OCLDynamicTypedBuffer<T, TScope>& var) : OCLVariable(var.name, var.bIsBlocking, var.accessType)
	{
		if (var.currentSize != 0)
		{
			value = (T*)malloc(var.currentSize * sizeof(T));
			currentSize = var.currentSize;
		}
		else
		{
			return;
		}
		if (var.value == NULL)
			return;

		for (int i = 0; i < var.currentSize; i++)
		{
			value[i] = var.value[i];
		}
	}

	OCLDynamicTypedBuffer<T, TScope>& operator =(const OCLDynamicTypedBuffer<T, TScope>& var)
	{
		return OCLDynamicTypedBuffer<T, TScope>(var);
	}

	virtual void* getValue() override { return value; };
	virtual void  setValue(void* val) override { for (int i = 0; i < currentSize; i++) value[i] = *(((T*)val) + i); bisUploaded = false; };
	virtual size_t getTypeSize() override { return sizeof(T); };
	virtual size_t getSize() override 
	{ 
		return currentSize * sizeof(T); 
	};
	T* getTypedValue() { return ((T*)(value)); };
	virtual EOCLBufferType getBufferType() override { return EOCLBufferType::BTNative; };
	operator OCLVariable*() const { return (OCLVariable*)this; };
	virtual T& operator[](size_t i) { return value[i]; };


	using _Mybase = std::_Vector_alloc<std::_Vec_base_types<T, std::allocator<T>>>;
	using iterator = typename _Mybase::iterator;

	_NODISCARD iterator begin() noexcept
	{	// return iterator for beginning of mutable sequence
		return (iterator(&this->value[0], _STD addressof(this->value[0])));
	}

	/*
	_NODISCARD iterator end() noexcept
	{	// return iterator for end of mutable sequence
		return (iterator(&this->value[currentSize], _STD addressof(this->value[currentSize])));
	}
	*/
	size_t getBufferLength()
	{
		return currentSize;
	}

	virtual void resizeBuffer(size_t size)
	{
		if (currentSize != size)
		{
			delete memoryBuffer;
			memoryBuffer = NULL;
		}
		else
		{
			return;
		}

		currentSize = size;
		if(value != NULL)
			delete value;
		value = (T*)malloc(currentSize * sizeof(T));
		bisUploaded = false;
	}

	virtual cl::Memory* getCLMemoryObject(cl::Context* context) override
	{
		if (context == NULL)
			return memoryBuffer;

		if (currentSize == 1 && TScope == ASPrivate)
			return NULL;

		if (memoryBuffer == NULL)
			memoryBuffer = new cl::Buffer(*context, getAccessType(), getSize());

		return memoryBuffer;
	};
};

//Base typed variable type
template<typename T, EOCLArgumentScope TScope = EOCLArgumentScope::ASGlobal, size_t size = 1>
class OCLTypedVariable : public OCLVariable
{
protected:
	cl::Buffer* memoryBuffer = NULL;
	bool bForceCLBuffer = false;
public:
	OCLTypedVariable(T* val = NULL, std::string name = "", bool bIsBlocking = true, EOCLAccessTypes accessType = EOCLAccessTypes::ATReadWrite) : OCLVariable(name, bIsBlocking, accessType)
	{
		if (val == NULL)
		{
			return;
		}

		for(int i = 0; i < size; i++)
			value[i] = val[i];
	}

	OCLTypedVariable(T val, std::string name = "", bool bIsBlocking = true, EOCLAccessTypes accessType = EOCLAccessTypes::ATReadWrite) : OCLVariable(name, bIsBlocking, accessType)
	{
		if(size != 1)
			throw OCLException("Size must be 1 in order to use this constructor!");

		value[0] = val;
	}

	OCLTypedVariable(const OCLTypedVariable<T, TScope, size>& var) : OCLVariable(var.name, var.bIsBlocking, var.accessType)
	{
		if (var.value == NULL)
		{
			return;
		}

		for (int i = 0; i < size; i++)
			value[i] = var.value[i];
	}

	OCLTypedVariable<T, TScope, size>& operator =(const OCLTypedVariable<T, TScope>& var)
	{
		return OCLTypedVariable<T, TScope, size>(var);
	}

	virtual ~OCLTypedVariable() override
	{
		if (memoryBuffer != NULL)
		{
			delete memoryBuffer;
		}

		OCLVariable::~OCLVariable();
	}

	void ForceCLBuffer() { bForceCLBuffer = true; };

	T value[size];
	virtual void* getValue() override { return &value[0]; };
	virtual void  setValue(void* val) override { for(int i = 0; i < size; i++) value[i] = *(((T*)val) + i); bisUploaded = false; bisUploaded = false; };
	virtual size_t getTypeSize() override { return sizeof(T); };
	virtual size_t getSize() override { return size * sizeof(T); };
	T* getTypedValue() { return ((T*)(&value[0])); };
	virtual bool needsCLBuffer() override { return bForceCLBuffer; };
	virtual EOCLBufferType getBufferType() override { return EOCLBufferType::BTNative; };
	operator OCLVariable*() const { return (OCLVariable*)this; };
	virtual T& operator[](size_t i) { return value[i]; }
	virtual cl::Memory* getCLMemoryObject(cl::Context* context) override 
	{
		if (context == NULL)
			return memoryBuffer;

		if (TScope == EOCLArgumentScope::ASPrivate)
			return NULL;

		if(memoryBuffer == NULL)
		   memoryBuffer = new cl::Buffer(*context, getAccessType(), getSize());

		return memoryBuffer;
	};
};

template<typename T, size_t size, EOCLArgumentScope TScope = EOCLArgumentScope::ASGlobal>
class OCLTypedRingBuffer : public OCLTypedVariable<T, TScope, size>
{
public:
	OCLTypedRingBuffer() : OCLTypedVariable<T, TScope, size>()
	{
		OCLTypedRingBuffer(NULL, 0, name, true);
	}
	/** Inits RingBuffer with DataSize amount of data */
	OCLTypedRingBuffer(T* val, size_t DataSize, std::string name = "", bool bIsBlocking = true, EOCLAccessTypes accessType = EOCLAccessTypes::ATReadWrite) : OCLTypedVariable<T, TScope, size>((T*)NULL, name, bIsBlocking, accessType)
	{
		currentBufferPos = DataSize;
		assert(DataSize <= size);

		CREATEMUTEX(updateMutex);

		if (val == NULL)
			return;

		for (int i = 0; i < DataSize; i++)
			value[i] = val[i];
	}

	virtual ~OCLTypedRingBuffer() override
	{
		DESTROYMUTEX(updateMutex);
		OCLTypedVariable<T, TScope, size>::~OCLTypedVariable();
	}

	virtual T& operator[] (size_t i) override
	{ 
		i = i % size;
		if (currentReadPos <= currentBufferPos)
		{
			if (i >= currentBufferPos)
				return T();
		}
		else
		{
			// -->-BP---i----RP->-- was forbidden..
			//if (i > currentBufferPos && i < currentReadPos)
			//	return T();
		}

		return value[i];
	}

	void setCurrentReadPos(size_t newPos)
	{
		currentReadPos = newPos;
	}

	void setCurrentWritePos(size_t newPos)
	{
		currentBufferPos = newPos;
	}

	void writeNext(T& val)
	{
		currentBufferPos = currentBufferPos % size;
		value[currentBufferPos] = val;
		++currentBufferPos;

		if (bisUploaded)
			bisUploaded = false;
	}

	void writeNext(T* val, size_t len)
	{
		for(size_t i = 0; i < len; i++)
		    value[(currentBufferPos + i) % size] = val;

		currentBufferPos += len;

		if (bisUploaded)
			bisUploaded = false;
	}

	size_t* getWriteBufferPtr()
	{
		return &currentBufferPos;
	}

	/** @Returns ref to next data*/
	T& readNext()
	{
		if (currentReadPos == currentBufferPos)
			return NULL;

		currentReadPos = currentReadPos % size;
		currentReadPos++;
		return value[currentReadPos-1];
	}

	virtual void* getValue() override
	{
		return (void*)readAll();
	}

	/** sets the current max reading position for uploading the ring buffer data
	    Needs to be set after host completes working on a segment of this buffer*/
	void setReadEndPosForCLDevice(size_t i)
	{
		ACQUIRE_MUTEX(updateMutex);
		readEndPosForCLDevice = i;
		RELEASE_MUTEX(updateMutex);
	}

	/** get size of all available data for cl device */
	virtual size_t getAvailableData() override 
	{ 
		ACQUIRE_MUTEX(updateMutex);
		if (readEndPosForCLDevice >= currentReadPos)
		{
			size_t retVal = (readEndPosForCLDevice - currentReadPos) * sizeof(T);
			RELEASE_MUTEX(updateMutex);
			return retVal;
		}

		size_t amount = size - currentReadPos;
		amount += readEndPosForCLDevice;
		RELEASE_MUTEX(updateMutex);
		return amount * sizeof(T);
	};

	/** read all data for cl device */
	T* readAll()
	{
		ACQUIRE_MUTEX(updateMutex);
		//virtualBufferPosOnCLDevice = (virtualBufferPosOnCLDevice + currentBufferPos) % size;
		//currentBufferPos = 0;
		currentReadPos = readEndPosForCLDevice;
		//readEndPosForCLDevice = currentBufferPos;
		RELEASE_MUTEX(updateMutex);
		return &value[0];
	}

	virtual cl_int uploadBuffer(cl::CommandQueue* queue) override
	{
		if (bisUploaded || getCLMemoryObject(NULL) == NULL || getAccessType() == ATWrite)
			return CL_SUCCESS;

		size_t lreadPos = currentReadPos;
		currentReadPos = readEndPosForCLDevice % size;

		if (lreadPos == readEndPosForCLDevice)
			return CL_SUCCESS;

		if (lreadPos <= readEndPosForCLDevice)
		{
			return queue->enqueueWriteBuffer(*(cl::Buffer*)getCLMemoryObject(NULL), getIsBlocking() ? CL_TRUE : CL_FALSE, lreadPos * sizeof(T), (readEndPosForCLDevice - lreadPos) * sizeof(T), &value[lreadPos]);
		}

		size_t amoutToEnd = size - lreadPos;
		cl_int errcode = queue->enqueueWriteBuffer(*(cl::Buffer*)getCLMemoryObject(NULL), getIsBlocking() ? CL_TRUE : CL_FALSE, lreadPos * sizeof(T), amoutToEnd * sizeof(T), &value[lreadPos]);
		if(readEndPosForCLDevice > 0)
			errcode |= queue->enqueueWriteBuffer(*(cl::Buffer*)getCLMemoryObject(NULL), getIsBlocking() ? CL_TRUE : CL_FALSE, 0, readEndPosForCLDevice * sizeof(T), &value[0]);

		if (errcode == CL_SUCCESS)
			bisUploaded = true;

		return errcode;
	}

	virtual size_t getDataOffset() override { return currentBufferPos /*virtualBufferPosOnCLDevice*/ * sizeof(T); };
	inline size_t getWriteIndex() { return currentBufferPos;/* virtualBufferPosOnCLDevice;*/ }
protected:
	size_t currentBufferPos = 0;
	size_t currentReadPos = 0;
//	size_t virtualBufferPosOnCLDevice = 0;
	size_t readEndPosForCLDevice = 0;
	MUTEXTYPE updateMutex;
};

template<typename T>
class OCLMemoryVariable : public OCLTypedVariable<T>
{
private:
	void* hostPtr = NULL;
	EOCLBufferType BufferType = EOCLBufferType::BTCLMem;
	EOCLAccessTypes HostAccess = EOCLAccessTypes::ATReadWrite;

public:
	OCLMemoryVariable() : OCLTypedVariable<T>()
	{
		assert((std::is_base_of<cl::Memory, T>()));

		if ((std::is_base_of<cl::Image, T>()))
			BufferType = EOCLBufferType::BTCLImage;
		if ((std::is_base_of<cl::ImageGL, T>()))
			BufferType = EOCLBufferType::BTGLImage;
	}

	OCLMemoryVariable(T* val, std::string name = "", bool bIsBlocking = true, EOCLAccessTypes accessType = EOCLAccessTypes::ATReadWrite, void* hostPointer = NULL) : OCLTypedVariable<T>(val, name, bIsBlocking, accessType)
	{
		assert((std::is_base_of<cl::Memory, T>()));

		if ((std::is_base_of<cl::Image, T>()))
			BufferType = EOCLBufferType::BTCLImage;
		if ((std::is_base_of<cl::ImageGL, T>()))
			BufferType = EOCLBufferType::BTGLImage;

		hostPtr = hostPointer;
	}

	OCLMemoryVariable(T val, std::string name = "", bool bIsBlocking = true, EOCLAccessTypes accessType = EOCLAccessTypes::ATReadWrite, void* hostPointer = NULL) : OCLTypedVariable<T>(val, name, bIsBlocking, accessType)
	{
		assert((std::is_base_of<cl::Memory, T>()));

		if ((std::is_base_of<cl::Image, T>()))
			BufferType = EOCLBufferType::BTCLImage;
		if ((std::is_base_of<cl::ImageGL, T>()))
			BufferType = EOCLBufferType::BTGLImage;

		hostPtr = hostPointer;
	}

	virtual bool needsCLBuffer() override { return false; };
	virtual EOCLBufferType getBufferType() override { return BufferType; };
	virtual void* getHostPointer() override { return hostPtr; };
	void setHostPointerMode(EOCLAccessTypes type)
	{
		HostAccess = type;
	}
	virtual void  setHostPointer(void* val) { hostPtr = val; };
	virtual cl::Memory* getCLMemoryObject(cl::Context* context) override
	{
		return (cl::Memory*) getValue();
	};

	virtual cl_int uploadBuffer(cl::CommandQueue* queue) override
	{
		if (getAccessType() == ATWrite || HostAccess == ATWrite)
			return CL_SUCCESS;

		if (getBufferType() == BTCLImage && getHostPointer() != NULL)
		{
			const cl::array<cl::size_type, 3> origin = { 0,0,0 };
			cl::Image* img = (cl::Image*)getValue();
			const cl::array<cl::size_type, 3> size = { img->getImageInfo<CL_IMAGE_WIDTH>(), img->getImageInfo<CL_IMAGE_HEIGHT>(), 1 };
			return queue->enqueueWriteImage(*img, (getIsBlocking()) ? CL_TRUE : CL_FALSE, origin, size, 0, 0, getHostPointer(), NULL, NULL);
		}

		throw OCLException("Trying to upload not implemented memory object!");
		return -1;
	}

	virtual cl_int downloadBuffer(cl::CommandQueue* queue) override
	{
		if (hostPtr == NULL)
		{
			return -1;
		}

		if (getBufferType() == BTCLImage && hostPtr != NULL)
		{
			const cl::array<cl::size_type, 3> origin = { 0,0,0 };
			cl::Image* img = (cl::Image*)getValue();
			const cl::array<cl::size_type, 3> size = { img->getImageInfo<CL_IMAGE_WIDTH>(), img->getImageInfo<CL_IMAGE_HEIGHT>(), 1 };
			return queue->enqueueReadImage(*img, getIsBlocking() ? CL_TRUE : CL_FALSE, origin, size, 0, 0, hostPtr, 0, 0);
		}

		throw OCLException("Trying to download unimplemented memory object!");
		return -1;
	}


	/** possible data type for init image is cl_uint4 */
	virtual cl_int initWithValue(cl::CommandQueue* queue, void* data, size_t size) override
	{
		switch (getBufferType())
		{
			case BTCLImage:
			{
				const cl::array<cl::size_type, 3> origin = { 0,0,0 };
				cl::Image* img = (cl::Image*)getValue();
				const cl::array<cl::size_type, 3> size = { img->getImageInfo<CL_IMAGE_WIDTH>(), img->getImageInfo<CL_IMAGE_HEIGHT>(), 1 };
				return queue->enqueueFillImage(*img, *(cl_uint4*)data, origin, size);
			}
		};

		throw OCLException("Trying to init unimplemented memory object!");

		return -1;
	}
};

typedef struct FOCLKernel
{
	size_t kernelID = 0;
	std::string mainMethodName;
	std::string source;
	cl::Program program;
	cl::Context* context = NULL;
	cl::Device* device = NULL;
	std::vector<OCLVariable*> Arguments;
	cl::NDRange globalThreadCount;
	/*WorkItem count per dimension*/
	cl::NDRange localThreadCount;
	cl::Kernel clKernel;

	FOCLKernel()
	{
		mainMethodName = "main_kernel";
		source = "";
		context = NULL;
		program = NULL;
		globalThreadCount = cl::NDRange(1);
		localThreadCount = cl::NullRange;
	}

	FOCLKernel(std::string source,
		std::vector<OCLVariable*> Arguments = {},
		cl::NDRange globalThreadCount = cl::NDRange(1),
		cl::NDRange localThreadCount = cl::NullRange)
	{
		this->source = source;
		this->context = NULL;
		this->program = NULL;
		this->Arguments = Arguments;
		this->globalThreadCount = globalThreadCount;
		this->localThreadCount = localThreadCount;
		mainMethodName = "main_kernel";
	}

	FOCLKernel(std::string mainMethodName, std::string source,
		std::vector<OCLVariable*> Arguments = {},
		cl::NDRange globalThreadCount = cl::NDRange(1),
		cl::NDRange localThreadCount = cl::NullRange)
	{
		this->mainMethodName = mainMethodName;
		this->source = source;
		this->context = NULL;
		this->program = NULL;
		this->Arguments = Arguments;
		this->globalThreadCount = globalThreadCount;
		this->localThreadCount = localThreadCount;
	}

} FOCLKernel;

typedef struct FOCLDeviceInfos
{
	size_t maxWorkGroupSize;
	long maxGlobalMemory;
	long maxDeviceMemory;
	bool imageSupport;
	size_t maxImage2DSize[2];
	int maxComputeUnits;
	cl_uint maxFrequency;
	long maxCLObjectSize;
	int maxWorkGroupDimensions;
	//length is maxWorkGroupDimensions
	std::vector<size_t> maxWorkItemsPerDimension;
	std::string deviceName;
	std::string clVersion;
	std::string vendor;

	FOCLDeviceInfos()
	{
		maxWorkGroupSize = 0;
		maxGlobalMemory = 0;
		maxDeviceMemory = 0;
		imageSupport  = 0;
		maxImage2DSize[0] = 0;
		maxImage2DSize[1] = 0;
		maxComputeUnits = 0;
		maxCLObjectSize = 0;
		maxWorkGroupDimensions = 0;
		maxFrequency = 0;
		deviceName = "None";
		clVersion = "0.0";
		vendor = "None";
	}

	FOCLDeviceInfos(cl::Device& p)
	{
		maxGlobalMemory = (long) p.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
		imageSupport = (p.getInfo<CL_DEVICE_IMAGE_SUPPORT>() == CL_TRUE);
		maxImage2DSize[0] = p.getInfo<CL_DEVICE_IMAGE2D_MAX_WIDTH>();
		maxImage2DSize[1] = p.getInfo<CL_DEVICE_IMAGE2D_MAX_HEIGHT>();
		maxDeviceMemory = (long)p.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();
		maxComputeUnits = p.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
		maxCLObjectSize = (long)p.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();
		maxWorkGroupSize = p.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
		maxWorkGroupDimensions = p.getInfo<CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS>();
		maxWorkItemsPerDimension = p.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>();
		maxFrequency = p.getInfo <CL_DEVICE_MAX_CLOCK_FREQUENCY>();
		std::stringstream s2;
		s2 << p.getInfo<CL_DEVICE_NAME>();
		deviceName = s2.str();
		std::stringstream s;
		s << p.getInfo<CL_DEVICE_VERSION>();
		clVersion = s.str();
		std::stringstream s3;
		s3 << p.getInfo<CL_DEVICE_VENDOR>();
		vendor = s3.str();
	}
}FOCLDeviceInfos;

inline bool operator==(FOCLKernel& lhs, FOCLKernel& rhs)
{
	return (lhs.kernelID == rhs.kernelID && lhs.kernelID > 0);
}

inline std::string clDecodeErrorCode(cl_int errcode)
{
	switch (errcode)
	{
	case CL_INVALID_CONTEXT: return "Invalid context";
	case CL_DEVICE_NOT_FOUND: return "Could not find any device!";
	case CL_INVALID_DEVICE: return "Invalid device";
	case CL_INVALID_VALUE: return "Invalid value";
	case CL_INVALID_QUEUE_PROPERTIES: return "properties are not supported by device";
	case CL_INVALID_COMMAND_QUEUE: return "the command queue is broken";
	case CL_OUT_OF_HOST_MEMORY: return "out of host memory";
	case CL_SUCCESS: return "";
	case CL_INVALID_HOST_PTR: return "Host pointer is invalid";
	case CL_IMAGE_FORMAT_NOT_SUPPORTED: return "The Image format is not supported!";
	case CL_INVALID_KERNEL_ARGS: return "One ore more kernel arguments are invalid!";
	case CL_INVALID_MEM_OBJECT: return "CL memory object is invalid!";
	case CL_OUT_OF_RESOURCES: return "CL out of ressources! Check memory or work items count.";
	case CL_INVALID_KERNEL: return "Try to access on undefined kernel!";
	case CL_INVALID_WORK_GROUP_SIZE: return "Workgroup size and size of kernelthreads is not divideable or the max workgroup size exceeded!";
	case CL_INVALID_ARG_VALUE: return "Argument was invalid on OpenCL Device!";
	case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR : return "Requested Imageformat is not supported by this device!";
	}

	std::stringstream ss;
	ss << "Unknown Error [" << std::to_string(errcode) << "]";
	return ss.str();
}

inline bool operator== (cl::NDRange& lhs, cl::NDRange& rhs)
{
	bool retVal = lhs.dimensions() == rhs.dimensions();
	if (!retVal)
		return false;

	for (size_t i = 0; i < lhs.dimensions(); i++)
	{
		retVal &= lhs[i] == rhs[i];
	}
	return retVal;
}

typedef struct FOCLKernelGroup
{
	FOCLKernel* kernel;
	//std::vector<cl::Buffer> dataBuffers;
	//std::vector<cl::Memory> dataBuffers;
	cl::CommandQueue* queue;
	bool bShouldBlockVariables;
	bool bIsRunning = false;
	bool bArgumentsWritten = false;
	const bool bIsChild = false;
	//MUTEXTYPE CL_LOCK;

	//Creates the group queue and initializes the data
	FOCLKernelGroup(const FOCLKernel& kernel, bool shouldBlockVariables = false)
		: bIsChild(false)
	{
		this->kernel = new FOCLKernel(kernel);
		bShouldBlockVariables = shouldBlockVariables;

		/*if (bAllowOutOfOrderExecution)
		{
			queue = new cl::CommandQueue(*kernel.context, kernel.device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
		}
		else
		{*/
			queue = new cl::CommandQueue(*kernel.context, *kernel.device);
		//}

		bArgumentsWritten = false;
		bIsRunning = false;
		//CREATEMUTEX(CL_LOCK);
	}

	
	FOCLKernelGroup(FOCLKernel& kernel, cl::CommandQueue* q, bool shouldBlockVariables = false)
		: bIsChild(true)
	{
		this->kernel = new FOCLKernel(kernel);
		bShouldBlockVariables = shouldBlockVariables;

		queue = q;

		bArgumentsWritten = false;
		bIsRunning = false;
	}

	FOCLKernelGroup()
		: FOCLKernelGroup(FOCLKernel())
	{

	}

	~FOCLKernelGroup()
	{
		if (bIsRunning)
		{
			for (int i = 0; i < kernel->Arguments.size(); i++)
				kernel->Arguments[i]->releaseCLMemory();
		}

		if(!bIsChild)
			delete queue;

		delete kernel;
	}

	void CleanUpDevice()
	{
//		clkernel.~Wrapper<cl_kernel>();
		if (!bIsChild)
			delete queue;
	}

	void UpdateVariable(size_t i)
	{
		cl_int errcode = CL_SUCCESS;
		kernel->Arguments[i]->getCLMemoryObject(kernel->context);

		errcode = kernel->Arguments[i]->uploadBuffer(queue);

		if (CL_SUCCESS != errcode)
		{
			std::printf("CL ERROR: could not write buffer[%zi] to CL device! [%s]\n", i, clDecodeErrorCode(errcode).c_str());
			if (errcode == CL_INVALID_COMMAND_QUEUE)
				throw OCLException("Can't continue with broken command queue!");
		}
	}

	void UploadArguments(bool sync = false, bool reUpload = false)
	{
		for (int i = 0; i < kernel->Arguments.size(); i++)
		{
			if(reUpload)
				kernel->Arguments[i]->setVariableChanged(true);

			UpdateVariable(i);
		}
		queue->flush();

		if (sync)
		{
			//wait for all memory operations to complete
			if (CL_SUCCESS != queue->enqueueBarrierWithWaitList())
				std::printf("CL ERROR: could not set up barrier on CL device!\n");
			cl_int errcode = queue->finish();
			if (CL_SUCCESS != errcode)
			{
				std::printf("CL ERROR: could not finish write buffers to CL device! [%s]\n", clDecodeErrorCode(errcode).c_str());
				if (errcode == CL_INVALID_COMMAND_QUEUE)
					throw OCLException("Can't continue with broken command queue!");
			}
		}

		bArgumentsWritten = true;
	}

	/** WaitforGroup is releasing all variables acquired by Run */
	void WaitForGroup(FOCLKernel* pkernel = NULL)
	{
		if (pkernel == NULL)
			pkernel = kernel;
		cl_int errcode = queue->finish();

		//checks if group was active before
		if(bIsRunning)
			for (int i = 0; i < pkernel->Arguments.size(); i++)
				pkernel->Arguments[i]->releaseCLMemory();

		bIsRunning = false;

		if (CL_SUCCESS != errcode)
		{
			std::printf("CL ERROR: could not finish wait for Work Group! [%s]\n", clDecodeErrorCode(errcode).c_str());
			if (errcode == CL_INVALID_COMMAND_QUEUE)
				throw OCLException("Can't continue with broken command queue!");
		}
	}

	void DownloadResult(OCLVariable* var)
	{
		cl_int errcode = var->downloadBuffer(queue);

		if (CL_SUCCESS != errcode)
			std::printf(" ERROR: could not read buffer with name: %s from CL device! [%s]\n", var->getName().c_str(), clDecodeErrorCode(errcode).c_str());
	}

	void DownloadResult(int i)
	{
		DownloadResult(kernel->Arguments[i]);
	}

	void DownloadResults(std::vector<OCLVariable*> vars)
	{
		for (int j = 0; j < vars.size(); j++)
		{
			DownloadResult(vars[j]);
		}
	}

	void DownloadResults()
	{
		for (int i = 0; i < kernel->Arguments.size(); i++)
		{
			DownloadResult(i);
		}
	}

    std::string printKernelArgInfos()
	{
		std::stringstream ss;
		ss << "Method: " << kernel->clKernel.getInfo<CL_KERNEL_FUNCTION_NAME>() << std::endl;
		ss << "Args:\n";
		for (int i = 0; i < kernel->Arguments.size(); i++)
		{
			ss << kernel->clKernel.getArgInfo<CL_KERNEL_ARG_TYPE_NAME>(i) << "  " << kernel->clKernel.getArgInfo<CL_KERNEL_ARG_NAME>(i) << "\n";
		}

		return ss.str();
	}

	size_t gcd(size_t n1, size_t n2) {
		return (n2 == 0) ? n1 : gcd(n2, n1 % n2);
	}

	/** WaitforGroup is needed to free all variables acquired by this call 
	    @param pkernel modified kernel for update of var
	*/
	void Run(const cl::vector<cl::Event>* events = NULL, cl::Event* event = NULL, FOCLKernel* pkernel = NULL)
	{
		if (pkernel == NULL)
			pkernel = kernel;
		else
		{
			*kernel = *pkernel;
			bArgumentsWritten = false;
		}

		if (bShouldBlockVariables)
		{
			for (int i = 0; i < pkernel->Arguments.size(); i++)
				pkernel->Arguments[i]->acquireCLMemory();
		}

		bIsRunning = true;

		if (!bArgumentsWritten)
		{
			UploadArguments(true, false);
		}

		cl_int err = CL_SUCCESS;

		for (int i = 0; i < pkernel->Arguments.size(); i++)
		{
			//upload primitives directly
			if (pkernel->Arguments[i]->getCLMemoryObject(pkernel->context) == NULL)
			{
				err = pkernel->clKernel.setArg(i, pkernel->Arguments[i]->getSize(), pkernel->Arguments[i]->getValue());
			}
			else
			{
				err = pkernel->clKernel.setArg(i, *pkernel->Arguments[i]->getCLMemoryObject(pkernel->context));
			}
			if (CL_SUCCESS != err)
				std::printf("CL ERROR: could not assign Argument(%i) to clKernel! [%s]\n", i, clDecodeErrorCode(err).c_str());
		}

		if (pkernel->localThreadCount.dimensions() == 0)
		{
			FOCLDeviceInfos info = FOCLDeviceInfos(*pkernel->device);
			pkernel->localThreadCount = pkernel->globalThreadCount;
			for (size_t i = 0; i < pkernel->localThreadCount.dimensions(); i++)
			{
				if (info.maxWorkItemsPerDimension[i] < pkernel->localThreadCount[i])
					pkernel->localThreadCount = gcd(info.maxWorkItemsPerDimension[i], pkernel->globalThreadCount[i]);
			}
		}

		err = queue->enqueueNDRangeKernel(pkernel->clKernel, cl::NDRange(0), pkernel->globalThreadCount, pkernel->localThreadCount, events, event);
		if (CL_SUCCESS != err)
			throw OCLException("CL ERROR: could not start clKernel!" + clDecodeErrorCode(err) + "\n  -> " + printKernelArgInfos() +"\n");
		err = queue->flush();
		if(CL_SUCCESS != err)
			throw OCLException("CL ERROR: could not start clKernel!" + clDecodeErrorCode(err) + "\n  -> " + printKernelArgInfos() + "\n");
	}
} FOCLKernelGroup;

#ifndef __USE_COMPILETIMERESSOURCES__
inline FOCLKernel __dynamicConstantsFill(FOCLKernel& kernel, std::vector<std::pair<std::string, std::string>> DynamicConstants = std::vector<std::pair<std::string, std::string>>())
{
	for (size_t i = 0; i < DynamicConstants.size(); i++)
	{
		ReplaceStringInPlace(kernel.source, "%" + DynamicConstants[i].first + "%", DynamicConstants[i].second);
	}

	return kernel;
}

inline FOCLKernel loadOCLKernel_helper(std::string name, std::string mainMethodName = "main_kernel")
{

	wchar_t* buf = (wchar_t*) malloc(255 * sizeof(wchar_t));
	_wgetcwd(buf, 255);
	std::wstring ws(buf);
	std::string cwd(ws.begin(), ws.end());
	std::string basepath = cwd + "/../libkatherine/TimePix/src/opencl/";
	std::ifstream t(basepath + name + ".cl");
	std::stringstream buffer;
	buffer << t.rdbuf();
	std::string source = buffer.str();

	std::smatch m;
	std::regex e("#include\\s*\"[\\w\\/\\\\.]{2,}\"");
	std::regex e2("\"[\\w\\/\\\\.]+\"");
	while (std::regex_search(source, m, e)) {
		for (auto x : m)
		{
			std::string include_line = x.str();
			std::smatch m2;
			std::regex_search(include_line, m2, e2);
			for (std::ssub_match x2 : m2)
			{
				std::string path = x2.str();
				path = path.substr(1, path.length() - 2);				
				if(fileExists(basepath + path))
					path = basepath + path;
				else
				{
					path = basepath + "include/" + path;
					if (!fileExists(path))
					{
						std::printf(" ERROR: Could not find cl include file: %s!\n", path.c_str());
						exit(-1);
					}
				}

				std::ifstream t2(path);
				std::stringstream buffer2;
				buffer2 << t2.rdbuf();
				ReplaceStringInPlace(source, include_line, buffer2.str() + "\n");
				break;
			}

		}
	};

	FOCLKernel kernel(mainMethodName, source);

	delete buf;

	return kernel;
}

#define loadOCLKernel(_name_, mainMethodName) loadOCLKernel_helper(#_name_, mainMethodName)
#define loadOCLKernelAndConstants(_name_, mainMethodName, DynamicConstants) __dynamicConstantsFill(loadOCLKernel_helper(#_name_, mainMethodName), DynamicConstants)
#endif

#ifdef __USE_COMPILETIMERESSOURCES__
inline std::string __dynamicConstantsFill(std::string& source, std::vector<std::pair<std::string, std::string>> DynamicConstants = std::vector<std::pair<std::string, std::string>>())
{
	for (size_t i = 0; i < DynamicConstants.size(); i++)
	{
		ReplaceStringInPlace(source, "%" + DynamicConstants[i].first + "%", DynamicConstants[i].second);
	}

	return source;
}

#define loadOCLKernel(_name_, mainMethodName) FOCLKernel(mainMethodName, CALL_OCL_METHOD(_name_))
#define loadOCLKernelAndConstants(_name_, mainMethodName, DynamicConstants) FOCLKernel(mainMethodName, __dynamicConstantsFill(CALL_OCL_METHOD(_name_), DynamicConstants))

#endif