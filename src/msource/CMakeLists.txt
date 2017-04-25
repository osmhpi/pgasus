
# Define source files
set(SOURCES
	malloc.c
	malloc-numa.h
	msource.cpp
	../../include/msource/msource.hpp
	../../include/msource/msource_types.hpp
	../../include/msource/msource_allocator.hpp
	../../include/msource/node_replicated.hpp
)

include_directories(.)
include_directories(..)
include_directories(../../include)
include_directories(../../include/msource)

add_definitions(-DHAVE_MMAP)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_definitions(-D_GNU_SOURCE)	# for mremap function
endif()

add_library(hpinuma_msource SHARED ${SOURCES})
target_link_libraries(hpinuma_msource hpinuma_util pthread numa)

if (PGASUS_BUILD_STATIC_LIBRARIES)
	add_library(hpinuma_msource_s STATIC ${SOURCES})
	target_link_libraries(hpinuma_msource_s hpinuma_util_s pthread numa)
endif()

