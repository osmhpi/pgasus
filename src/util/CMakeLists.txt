
# Define source files
set(SOURCES
	../../include/base/node.hpp
	../../include/base/ref_ptr.hpp
	../../include/base/spinlock.hpp
        ../../include/base/rwlock.hpp
        ../../include/msource/mmaphelper.h
        ../../include/synced_containers.hpp
	debug.cpp
	debug.hpp
	node.cpp
	mmaphelper.cpp
	strutil.cpp
        strutil.hpp
	timer.hpp
	topology.cpp
	topology.hpp
	tsc.hpp
)

include_directories(..)
include_directories(../../include)

add_library(hpinuma_util SHARED ${SOURCES})
target_link_libraries(hpinuma_util hwloc)

if (PGASUS_BUILD_STATIC_LIBRARIES)
	add_library(hpinuma_util_s STATIC ${SOURCES})
        target_link_libraries(hpinuma_util_s hwloc)
endif()
