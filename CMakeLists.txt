cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


project(LibNumaCpp C CXX)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LINUX 1)
endif()

if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "ppc64le")
    message("Detected a POWER system")
    set(PPC64LE 1)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        -Wpedantic
        -Wall
        -Wno-missing-braces
    )
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "4.0")
        add_compile_options(
            $<$<CONFIG:Debug>:-fno-limit-debug-info>
        )
    endif()

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Wpedantic
        -Wall
    )

    if (NOT PPC64LE)
        add_compile_options(
            -march=native
        )
    endif()

    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "5.0")
        add_definitions(-D_GLIBCXX_USE_CXX11_ABI)
    endif()

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    add_compile_options(-Wall $<$<CONFIG:Release>:-Ofast -no-prec-div -xHOST>)
endif()


option(PGASUS_BUILD_STATIC_LIBRARIES "Also build static libraries" ON)

option(PGASUS_RDTSC_PROFILING "Also rdtsc()-based profiling for msources and spinlocks" OFF)
if (PGASUS_RDTSC_PROFILING)
	add_definitions(-DNUMA_PROFILE_SPINLOCK=1)
	add_definitions(-DNUMA_PROFILE_MSOURCE=1)
endif()

option(PGASUS_DEBUG_MALLOC "Annotate freshly allocated and freed memory regions with special values (0xAA, 0xBB, 0xCC)" OFF)
if (PGASUS_DEBUG_MALLOC)
	add_definitions(-DMEM_SOURCE_FILL_MEMORY_DEBUG=1)
endif()


# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Boost 1.56 COMPONENTS context chrono thread system REQUIRED)
if (Boost_VERSION VERSION_GREATER 106000)
	message(FATAL_ERROR "Boost version newer than 1.60 is currently not supported.")
endif()
find_package(ZLIB REQUIRED)
find_package(NUMA REQUIRED)
find_package(HWLOC REQUIRED)

include_directories(${Boost_INCLUDE_DIR})

# add sub-projects
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(docs)
