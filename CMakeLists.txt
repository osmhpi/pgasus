#define Project
cmake_minimum_required(VERSION 2.6)
project(LibNumaCpp)

set(COMPILER_TYPE "gcc" CACHE STRING "Choose thy compiler.")
set_property(CACHE COMPILER_TYPE PROPERTY STRINGS gcc gcc5 intel clang)

set(COMPILER_FLAGS "" CACHE STRING "Additional compiler flags")

if (COMPILER_TYPE STREQUAL "clang")
	set(CMAKE_C_COMPILER clang)
	set(CMAKE_CXX_COMPILER clang++)
	set(FFAST "-O3")
	set(FBASE "-Wall")
elseif (COMPILER_TYPE STREQUAL "intel")
	set(CMAKE_C_COMPILER icc)
	set(CMAKE_CXX_COMPILER icc)
	set(FFAST "-Ofast -no-prec-div -xHOST")
	set(FBASE "-Wall")
elseif (COMPILER_TYPE STREQUAL "gcc5")
	set(CMAKE_C_COMPILER gcc-5)
	set(CMAKE_CXX_COMPILER g++-5)
	set(FBASE "-Wpedantic -Wall")
	set(FFAST "-O3 -march=native")
	add_definitions(-D_GLIBCXX_USE_CXX11_ABI)
else ()
	set(CMAKE_C_COMPILER gcc)
	set(CMAKE_CXX_COMPILER g++-4.9)
	set(FFAST "-O3 -march=native")
	set(FBASE "-Wpedantic -Wall")
endif()

# User-provided definitions
set(FBASE "${FBASE} ${COMPILER_FLAGS}")

set(FLAGS_DEBUG "${FBASE} -g")
set(FLAGS_RELDBG "${FBASE} ${FFAST} -g")
set(FLAGS_REL "${FBASE} ${FFAST}")

set(CMAKE_C_FLAGS "${FBASE}")
set(CMAKE_C_FLAGS_DEBUG "${FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${FLAGS_RELDBG}")
set(CMAKE_C_FLAGS_RELEASE "${FLAGS_REL}")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -std=c++11")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -std=c++11")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -std=c++11")

option(BUILD_STATIC_LIBRARIES "Also build static libraries" ON)

option(RDTSC_PROFILING "Also rdtsc()-based profiling for msources and spinlocks" OFF)
if (RDTSC_PROFILING)
	add_definitions(-DNUMA_PROFILE_SPINLOCK=1)
	add_definitions(-DNUMA_PROFILE_MSOURCE=1)
endif()

option(DEBUG_MALLOC "Annotate freshly allocated and freed memory regions with special values (0xAA, 0xBB, 0xCC)" OFF)
if (DEBUG_MALLOC)
	add_definitions(-DMEM_SOURCE_FILL_MEMORY_DEBUG=1)
endif()


# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Boost 1.56 COMPONENTS context chrono thread system REQUIRED)
find_package(ZLIB REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

# add sub-projects
add_subdirectory(src)
add_subdirectory(test)
