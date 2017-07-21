
set(PUBLIC_HEADERS
	${PROJECT_INCLUDE_DIR}/PGASUS/base/node.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/base/ref_ptr.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/base/spinlock.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/base/tsc.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/base/rwlock.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/synced_containers.hpp
)

set(SOURCES
	${PUBLIC_HEADERS}
	debug.cpp
	debug.hpp
	node.cpp
	strutil.cpp
	strutil.hpp
	timer.hpp
	topology.cpp
	topology.hpp
)

set(include_dirs
	PUBLIC
		$<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
		$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
		$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>
	PRIVATE
		${PROJECT_SOURCE_DIR}/src
)

add_library(hpinuma_base SHARED ${SOURCES})
target_link_libraries(hpinuma_base
	PUBLIC
		hwloc
		numa
)
target_include_directories(hpinuma_base ${include_dirs})
if (DEFAULT_COMPILE_DEFINITIONS)
	target_compile_definitions(hpinuma_base ${DEFAULT_COMPILE_DEFINITIONS})
endif()

# Create public exports
set(HPINUMA_BASE_EXPORT_HEADER "${PROJECT_BINARY_DIR}/src/PGASUS/base/hpinuma_base_export.h")
generate_export_header(hpinuma_base EXPORT_FILE_NAME ${HPINUMA_BASE_EXPORT_HEADER})
list(APPEND PUBLIC_HEADERS ${HPINUMA_BASE_EXPORT_HEADER})
# set_target_properties(hpinuma_base PROPERTIES
#   PUBLIC_HEADER "${PUBLIC_HEADERS}")
addExportTarget(hpinuma_base)
install(TARGETS hpinuma_base
    EXPORT ${package_name}Targets COMPONENT dev
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
)
install(FILES ${HPINUMA_BASE_EXPORT_HEADER}
	DESTINATION "${INSTALL_INCLUDE_DIR}/PGASUS/base" COMPONENT dev)

if (PGASUS_BUILD_STATIC_LIBRARIES)
	add_library(hpinuma_base_s STATIC ${SOURCES})
	target_link_libraries(hpinuma_base_s
		PUBLIC
			hwloc
			numa
	)
	target_include_directories(hpinuma_base_s ${include_dirs})
	target_compile_definitions(hpinuma_base_s ${DEFAULT_COMPILE_DEFINITIONS}
		PUBLIC HPINUMA_BASE_STATIC_DEFINE)

	# set_target_properties(hpinuma_base_s PROPERTIES
	# 	PUBLIC_HEADER "${PUBLIC_HEADERS}")
	addExportTarget(hpinuma_base_s)
	install(TARGETS hpinuma_base_s
		EXPORT ${package_name}Targets COMPONENT dev
		ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT staticlib
	)

endif()

add_executable(hpinuma_util_tool
	hpinuma_util_tool.cpp)
if (DEFAULT_COMPILE_DEFINITIONS)
	target_compile_definitions(hpinuma_util_tool ${DEFAULT_COMPILE_DEFINITIONS})
endif()
target_include_directories(hpinuma_util_tool ${include_dirs})
if (PGASUS_BUILD_STATIC_LIBRARIES)
	target_link_libraries(hpinuma_util_tool hpinuma_base_s)
else()
	target_link_libraries(hpinuma_util_tool hpinuma_base)
endif()
