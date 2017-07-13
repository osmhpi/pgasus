#define Project
cmake_minimum_required(VERSION 2.6)
project(LibNumaCpp)

set_property(CACHE PROPERTY STRINGS gcc gcc5 intel clang)

set(COMPILER_FLAGS "" CACHE STRING "Additional compiler flags")

if (CMAKE_C_COMPILER STREQUAL "clang")
	set(FFAST "-O3")
	set(FBASE "-Wall")
else ()
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

set(SOURCES
    include/malloc.hpp
    include/msource/msource_types.hpp
    include/msource/msource_allocator.hpp
    include/msource/node_replicated.hpp
    include/msource/msource.hpp
    src/stackedmalloc/stackedmalloc.cpp
    src/util/mmaphelper.cpp
    src/util/strutil.cpp
    src/util/topology.cpp
    src/util/debug.cpp
    src/util/node.cpp
    src/msource/malloc.c
    src/msource/msource.cpp

)

include_directories(./src)
include_directories(./include)
include_directories(./include/msource)

add_definitions(-DHAVE_MMAP)

# Create Shared Library
add_library(pgasus SHARED ${SOURCES})
target_link_libraries(pgasus pthread numa)

# Create Static Library
add_library(pgasus_s STATIC ${SOURCES})
target_link_libraries(pgasus_s pthread numa)

