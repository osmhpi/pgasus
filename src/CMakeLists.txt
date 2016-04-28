#define Project
cmake_minimum_required(VERSION 2.6)

add_subdirectory(util)
add_subdirectory(msource)

# main library
set(SOURCES
	../include/tasking/synchronizable.hpp
	../include/tasking/task.hpp
	../include/tasking/tasking.hpp
	../include/mutex.hpp
	../include/base/node.hpp
	../include/base/spinlock.hpp
	../include/base/rwlock.hpp
	../include/base/ref_ptr.hpp
	../include/init.hpp
	../include/malloc.hpp
	../include/msource/msource_types.hpp
	../include/msource/msource_allocator.hpp
	../include/msource/node_replicated.hpp
	../include/msource/msource.hpp
	../include/hashtable/containers.hpp
	../include/hashtable/hashtable.hpp
	../include/hashtable/hashtable_lockfree.hpp
	../include/hashtable/rwlock.hpp
	stackedmalloc/stackedmalloc.cpp
	tasking/context.hpp
	tasking/task_base.cpp
	tasking/task_collection.cpp
	tasking/task_collection.hpp
	tasking/task_interface.cpp
	tasking/task_scheduler.cpp
	tasking/task_scheduler.hpp
	tasking/thread_manager.cpp
	tasking/thread_manager.hpp
	tasking/worker_thread.cpp
	tasking/worker_thread.hpp
)

include_directories(.)
include_directories(../include)

# Create Library
add_library(hpinuma SHARED ${SOURCES})
target_link_libraries(hpinuma 
	hpinuma_msource hpinuma_util
	${Boost_CONTEXT_LIBRARY_RELEASE}
	${Boost_CHRONO_LIBRARY_RELEASE}
	${Boost_SYSTEM_LIBRARY_RELEASE}
	${Boost_THREAD_LIBRARY_RELEASE}
)

# Create Library
if (BUILD_STATIC_LIBRARIES)
	add_library(hpinuma_s STATIC ${SOURCES})
	target_link_libraries(hpinuma_s
		hpinuma_msource_s hpinuma_util_s
		${Boost_CONTEXT_LIBRARY_RELEASE}
		${Boost_CHRONO_LIBRARY_RELEASE}
		${Boost_SYSTEM_LIBRARY_RELEASE}
		${Boost_THREAD_LIBRARY_RELEASE}
	)
endif()
