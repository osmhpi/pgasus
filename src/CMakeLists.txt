
set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

add_subdirectory(base)
add_subdirectory(msource)

set(PUBLIC_HEADERS
	${PROJECT_INCLUDE_DIR}/PGASUS/mutex.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/hashtable/containers.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/hashtable/hashtable.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/hashtable/hashtable_lockfree.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/hashtable/rwlock.hpp
)

if (PGASUS_WITH_TASKING)
	list(APPEND PUBLIC_HEADERS
		${PROJECT_INCLUDE_DIR}/PGASUS/tasking/synchronizable.hpp
		${PROJECT_INCLUDE_DIR}/PGASUS/tasking/task.hpp
		${PROJECT_INCLUDE_DIR}/PGASUS/tasking/tasking.hpp
	)
endif()


# main library
set(SOURCES
	${PUBLIC_HEADERS}
)

if (PGASUS_WITH_TASKING)
	list(APPEND SOURCES
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
endif()

set(include_dirs
	PRIVATE
		${PROJECT_SOURCE_DIR}/src
)

if (PGASUS_WITH_TASKING)
	list(APPEND include_dirs
		PUBLIC
			${Boost_INCLUDE_DIRS}
	)
endif()

# Create Library
add_library(PGASUS SHARED ${SOURCES})
set_target_properties(PGASUS PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(PGASUS
	PUBLIC
		PGASUS_base
		PGASUS_msource
)
target_include_directories(PGASUS ${include_dirs})
if (PGASUS_WITH_TASKING)
	target_link_libraries(PGASUS PUBLIC ${Boost_LIBRARIES})
endif()
cppcheck_target(PGASUS)

# Create public exports
set(PGASUS_EXPORT_HEADER "${PROJECT_BINARY_DIR}/src/PGASUS/PGASUS_export.h")
generate_export_header(PGASUS EXPORT_FILE_NAME ${PGASUS_EXPORT_HEADER})
list(APPEND PUBLIC_HEADERS ${PGASUS_EXPORT_HEADER})
# set_target_properties(PGASUS PROPERTIES
#   PUBLIC_HEADER "${PUBLIC_HEADERS}")
addExportTarget(PGASUS)
install(TARGETS PGASUS
	EXPORT ${package_name}Targets COMPONENT dev
	RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
	LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
)
install(FILES ${PGASUS_EXPORT_HEADER} DESTINATION "${INSTALL_INCLUDE_DIR}/PGASUS" COMPONENT dev)

add_sanitizers(PGASUS)


# Create Library
if (PGASUS_BUILD_STATIC_LIBRARIES)
	add_library(PGASUS_s STATIC ${SOURCES})
	set_target_properties(PGASUS_s PROPERTIES LINKER_LANGUAGE CXX)
	target_link_libraries(PGASUS_s
		PUBLIC
			PGASUS_base_s
			PGASUS_msource_s
	)
	if (PGASUS_WITH_TASKING)
		target_link_libraries(PGASUS_s PUBLIC ${Boost_LIBRARIES})
	endif()
	target_include_directories(PGASUS_s ${include_dirs})
	target_compile_definitions(PGASUS_s ${DEFAULT_COMPILE_DEFINITIONS}
		PUBLIC HPINUMA_STATIC_DEFINE)

	# set_target_properties(PGASUS_s PROPERTIES
	# 	PUBLIC_HEADER "${PUBLIC_HEADERS}")
	addExportTarget(PGASUS_s)
	install(TARGETS PGASUS_s
		EXPORT ${package_name}Targets COMPONENT dev
		ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT staticlib
	)
	add_sanitizers(PGASUS_s)
endif()
