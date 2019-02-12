//**** DEFINITIONS REPLACED BY CMAKE OPTIONS ****//

//The main header that should be included first in any .h file in order to keep all defines
#pragma once
//#define __SIMULATION__  //tells the compiler to use the simulation code 
//#define __USE_OPENCL__  //tells the compiler to use OpenCL via the SYCL lib

#define PixelDataBufferSize 650000

#define _USE_MATH_DEFINES

//#define CL_HPP_TARGET_OPENCL_VERSION 200
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120

//#define USE_DEBUG_RENDERS