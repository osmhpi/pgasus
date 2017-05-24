
set(PUBLIC_HEADERS
	../../include/malloc.hpp
	../../include/msource/mmaphelper.h
	../../include/msource/msource.hpp
	../../include/msource/msource_allocator.hpp
	../../include/msource/msource_types.hpp
	../../include/msource/node_replicated.hpp
	../../include/msource/singleton.hpp
)

set(SOURCES
	${PUBLIC_HEADERS}
	malloc.c
	malloc-numa.h
	mmaphelper.cpp
	msource.cpp
)

set(include_dirs
	PRIVATE
		${PROJECT_SOURCE_DIR}/src
)

add_library(hpinuma_msource SHARED ${SOURCES})
target_link_libraries(hpinuma_msource
	PUBLIC
		hpinuma_util
		pthread
		numa
)
target_include_directories(hpinuma_msource ${include_dirs})
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_compile_definitions(hpinuma_msource
		PRIVATE _GNU_SOURCE)	# for mremap function
endif()

# Create public exports
generate_export_header(hpinuma_msource)
list(APPEND PUBLIC_HEADERS "${CMAKE_CURRENT_BINARY_DIR}/hpinuma_msource_export.h")
set_target_properties(hpinuma_msource PROPERTIES
  PUBLIC_HEADER "${PUBLIC_HEADERS}")
addExportTarget(hpinuma_msource)
install(TARGETS hpinuma_msource
    EXPORT ${package_name}Targets COMPONENT dev
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
    PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}" COMPONENT dev
)


if (PGASUS_BUILD_STATIC_LIBRARIES)
	add_library(hpinuma_msource_s STATIC ${SOURCES})
	target_link_libraries(hpinuma_msource_s
		PUBLIC
			hpinuma_util_s
			pthread
			numa
	)
	target_include_directories(hpinuma_msource_s ${include_dirs})
	if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
		target_compile_definitions(hpinuma_msource_s
			PRIVATE _GNU_SOURCE)	# for mremap function
	endif()
	target_compile_definitions(hpinuma_msource_s ${DEFAULT_COMPILE_DEFINITIONS}
		PUBLIC HPINUMA_MSOURCE_STATIC_DEFINE)

	set_target_properties(hpinuma_msource_s PROPERTIES
		PUBLIC_HEADER "${PUBLIC_HEADERS}")
	addExportTarget(hpinuma_msource_s)
	install(TARGETS hpinuma_msource_s
		EXPORT ${package_name}Targets COMPONENT dev
		ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT staticlib
		PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}" COMPONENT dev
	)
endif()
