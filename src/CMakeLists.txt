
add_subdirectory(util)
add_subdirectory(msource)


set(PUBLIC_HEADERS
	../include/tasking/synchronizable.hpp
	../include/tasking/task.hpp
	../include/tasking/tasking.hpp
	../include/mutex.hpp
	../include/hashtable/containers.hpp
	../include/hashtable/hashtable.hpp
	../include/hashtable/hashtable_lockfree.hpp
	../include/hashtable/rwlock.hpp
)


# main library
set(SOURCES
	${PUBLIC_HEADERS}
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
	PUBLIC
		${Boost_INCLUDE_DIRS}
)

# Create Library
add_library(hpinuma SHARED ${SOURCES})
target_link_libraries(hpinuma
	PUBLIC
		hpinuma_msource
		hpinuma_util
		${Boost_LIBRARIES}
)
target_include_directories(hpinuma ${include_dirs})

# Create public exports
generate_export_header(hpinuma)
list(APPEND PUBLIC_HEADERS "${CMAKE_CURRENT_BINARY_DIR}/hpinuma_export.h")
set_target_properties(hpinuma PROPERTIES
  PUBLIC_HEADER "${PUBLIC_HEADERS}")
addExportTarget(hpinuma)
install(TARGETS hpinuma
	EXPORT ${package_name}Targets COMPONENT dev
	RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
	LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
	PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}" COMPONENT dev
)


# Create Library
if (PGASUS_BUILD_STATIC_LIBRARIES)
	add_library(hpinuma_s STATIC ${SOURCES})
	target_link_libraries(hpinuma_s
		PUBLIC
			hpinuma_msource_s
			hpinuma_util_s
			${Boost_LIBRARIES}
	)
	target_include_directories(hpinuma_s ${include_dirs})
	target_compile_definitions(hpinuma_s ${DEFAULT_COMPILE_DEFINITIONS}
		PUBLIC HPINUMA_STATIC_DEFINE)

	set_target_properties(hpinuma_s PROPERTIES
		PUBLIC_HEADER "${PUBLIC_HEADERS}")
	addExportTarget(hpinuma_s)
	install(TARGETS hpinuma_s
		EXPORT ${package_name}Targets COMPONENT dev
		ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT staticlib
		PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}" COMPONENT dev
	)
endif()
