
set(PUBLIC_HEADERS
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
target_compile_definitions(hpinuma_msource PRIVATE HAVE_MMAP)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_compile_definitions(hpinuma_msource
		PRIVATE _GNU_SOURCE)	# for mremap function
endif()

# Create public exports
set_target_properties(hpinuma_msource PROPERTIES
  PUBLIC_HEADER "${PUBLIC_HEADERS}")
set(export_targets ${export_targets};hpinuma_msource PARENT_SCOPE)
install(TARGETS hpinuma_msource
    EXPORT ${package_name}Targets
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
	target_compile_definitions(hpinuma_msource_s PRIVATE HAVE_MMAP)
	if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
		target_compile_definitions(hpinuma_msource_s
			PRIVATE _GNU_SOURCE)	# for mremap function
	endif()
endif()
