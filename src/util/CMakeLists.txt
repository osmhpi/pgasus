
set(PUBLIC_HEADERS
	../../include/base/node.hpp
	../../include/base/ref_ptr.hpp
	../../include/base/spinlock.hpp
	../../include/base/tsc.hpp
	../../include/base/rwlock.hpp
	../../include/synced_containers.hpp
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
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
		$<INSTALL_INTERFACE:install>
	PRIVATE
		${PROJECT_SOURCE_DIR}/src
)

add_library(hpinuma_util SHARED ${SOURCES})
target_link_libraries(hpinuma_util
	PUBLIC hwloc)
target_include_directories(hpinuma_util ${include_dirs})

# Create public exports
set_target_properties(hpinuma_util PROPERTIES
  PUBLIC_HEADER "${PUBLIC_HEADERS}")
addExportTarget(hpinuma_util)
install(TARGETS hpinuma_util
    EXPORT ${package_name}Targets
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
    PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}" COMPONENT dev
)

if (PGASUS_BUILD_STATIC_LIBRARIES)
	add_library(hpinuma_util_s STATIC ${SOURCES})
	target_link_libraries(hpinuma_util_s
		PUBLIC hwloc)
	target_include_directories(hpinuma_util_s ${include_dirs})

	set_target_properties(hpinuma_util_s PROPERTIES
		PUBLIC_HEADER "${PUBLIC_HEADERS}")
	addExportTarget(hpinuma_util_s)
	install(TARGETS hpinuma_util_s
		EXPORT ${package_name}Targets
		ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT staticlib
		PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}" COMPONENT dev
	)
endif()
