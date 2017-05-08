
add_subdirectory(util)
add_subdirectory(msource)


set(PUBLIC_HEADERS
	../include/tasking/synchronizable.hpp
	../include/tasking/task.hpp
	../include/tasking/tasking.hpp
	../include/mutex.hpp
	../include/init.hpp
	../include/malloc.hpp
	../include/hashtable/containers.hpp
	../include/hashtable/hashtable.hpp
	../include/hashtable/hashtable_lockfree.hpp
	../include/hashtable/rwlock.hpp
)


# main library
set(SOURCES
	${PUBLIC_HEADERS}
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

set(include_dirs
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

# Create Library
add_library(hpinuma SHARED ${SOURCES})
target_link_libraries(hpinuma 
	hpinuma_msource hpinuma_util
	Boost::context
	Boost::chrono
	Boost::system
	Boost::thread
)
target_include_directories(hpinuma ${include_dirs})

# Create public exports
set_target_properties(hpinuma PROPERTIES
  PUBLIC_HEADER "${PUBLIC_HEADERS}")
addExportTarget(hpinuma)
install(TARGETS hpinuma
    EXPORT ${package_name}Targets
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
    PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}" COMPONENT dev
)


# Create Library
if (PGASUS_BUILD_STATIC_LIBRARIES)
	add_library(hpinuma_s STATIC ${SOURCES})
	target_link_libraries(hpinuma_s
		hpinuma_msource_s hpinuma_util_s
		Boost::context
		Boost::chrono
		Boost::system
		Boost::thread
	)
	target_include_directories(hpinuma_s ${include_dirs})

	set_target_properties(hpinuma_s PROPERTIES
		PUBLIC_HEADER "${PUBLIC_HEADERS}")
	addExportTarget(hpinuma_s)
	install(TARGETS hpinuma_s
		EXPORT ${package_name}Targets
		ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT staticlib
		PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}" COMPONENT dev
	)
endif()
