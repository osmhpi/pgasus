
set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

add_subdirectory(base)
add_subdirectory(msource)

set(PUBLIC_HEADERS
	${PROJECT_INCLUDE_DIR}/PGASUS/tasking/synchronizable.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/tasking/task.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/tasking/tasking.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/mutex.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/hashtable/containers.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/hashtable/hashtable.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/hashtable/hashtable_lockfree.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/hashtable/rwlock.hpp
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
		hpinuma_base
		hpinuma_msource
		${Boost_LIBRARIES}
)
target_include_directories(hpinuma ${include_dirs})

# Create public exports
set(HPINUMA_EXPORT_HEADER "${PROJECT_BINARY_DIR}/src/PGASUS/hpinuma_export.h")
generate_export_header(hpinuma EXPORT_FILE_NAME ${HPINUMA_EXPORT_HEADER})
list(APPEND PUBLIC_HEADERS ${HPINUMA_EXPORT_HEADER})
# set_target_properties(hpinuma PROPERTIES
#   PUBLIC_HEADER "${PUBLIC_HEADERS}")
addExportTarget(hpinuma)
install(TARGETS hpinuma
	EXPORT ${package_name}Targets COMPONENT dev
	RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
	LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
)
install(FILES ${HPINUMA_EXPORT_HEADER} DESTINATION "${INSTALL_INCLUDE_DIR}/PGASUS" COMPONENT dev)

add_sanitizers(hpinuma)


# Create Library
if (PGASUS_BUILD_STATIC_LIBRARIES)
	add_library(hpinuma_s STATIC ${SOURCES})
	target_link_libraries(hpinuma_s
		PUBLIC
			hpinuma_base_s
			hpinuma_msource_s
			${Boost_LIBRARIES}
	)
	target_include_directories(hpinuma_s ${include_dirs})
	target_compile_definitions(hpinuma_s ${DEFAULT_COMPILE_DEFINITIONS}
		PUBLIC HPINUMA_STATIC_DEFINE)

	# set_target_properties(hpinuma_s PROPERTIES
	# 	PUBLIC_HEADER "${PUBLIC_HEADERS}")
	addExportTarget(hpinuma_s)
	install(TARGETS hpinuma_s
		EXPORT ${package_name}Targets COMPONENT dev
		ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT staticlib
	)
	add_sanitizers(hpinuma_s)
endif()
