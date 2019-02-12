#pragma once
#ifdef __USE_SYCL__
#include <CL/sycl.hpp>

#define SYCL_READ cl::sycl::access::mode::read
#define SYCL_WRITE cl::sycl::access::mode::write
#define SYCL_READ_WRITE cl::sycl::access::mode::read_write

template<typename T, cl::sycl::access::mode mode>
using FHostBufferAccess = cl::sycl::accessor<T, 1, mode, cl::sycl::access::target::host_buffer>;
template<typename T, cl::sycl::access::mode mode>
using FGlobalBufferAccess = cl::sycl::accessor<T, 1, mode, cl::sycl::access::target::global_buffer>;

#else

#endif