
set(PGASUS_REPLACE_MALLOC ON CACHE BOOL
	"Determines whether PGASUS replaces the process-wide malloc function.")

set(PUBLIC_HEADERS
	${PROJECT_INCLUDE_DIR}/PGASUS/msource/mmaphelper.h
	${PROJECT_INCLUDE_DIR}/PGASUS/msource/msource.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/msource/msource_allocator.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/msource/msource_types.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/msource/node_replicated.hpp
	${PROJECT_INCLUDE_DIR}/PGASUS/msource/singleton.hpp
)

if(PGASUS_REPLACE_MALLOC)
	set(PUBLIC_HEADERS 
		${PUBLIC_HEADERS}
		${PROJECT_INCLUDE_DIR}/PGASUS/malloc.hpp
	)
endif()

set(SOURCES
	${PUBLIC_HEADERS}
	mmaphelper.cpp
	msource.cpp
)

if(PGASUS_REPLACE_MALLOC)
	set(SOURCES 
		${SOURCES}
		stackedmalloc.cpp
	)
endif()

set(include_dirs
	PRIVATE
		${PROJECT_SOURCE_DIR}/src
)


set(PGASUS_MALLOC_IMPLS "ptmalloc3" "dlmalloc")
set(default_malloc_impl "ptmalloc3")
set(PGASUS_MALLOC_IMPL ${default_malloc_impl} CACHE STRING
	"Malloc implementation that is used internally for PGASUS. Valid values: ${PGASUS_MALLOC_IMPLS}")
set_property(CACHE PGASUS_MALLOC_IMPL PROPERTY STRINGS ${PGASUS_MALLOC_IMPLS})


set(MALLOC_HEADER)
set(MALLOC_SOURCES)
set(MALLOC_DEFINES)

if (PGASUS_MALLOC_IMPL STREQUAL "ptmalloc3")
	set(MALLOC_HEADER ptmalloc3-malloc-numa.h)
	set(MALLOC_SOURCES ptmalloc3-malloc.c)
	if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
		set(MALLOC_DEFINES PRIVATE _GNU_SOURCE)	# for mremap function
	endif()

elseif(PGASUS_MALLOC_IMPL STREQUAL "dlmalloc")
	set(MALLOC_HEADER dlmalloc-malloc-numa.h)
	set(MALLOC_SOURCES dlmalloc-malloc.c)

else()
	string(REPLACE ";" ", " impls_msg "${PGASUS_MALLOC_IMPLS}")
	message(FATAL_ERROR "Invalid value for PGASUS_MALLOC_IMPL: \"${PGASUS_MALLOC_IMPL}\" \
Supported values are: ${impls_msg}.")
endif()

file(RELATIVE_PATH CURRENT_LIST_FILE_RELATIVE ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_LIST_FILE})
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/malloc-numa.h
	"#pragma once\n\n// Generated in ${CURRENT_LIST_FILE_RELATIVE}\n\
// Don't modify this file directly!\n\n\
#include \"msource/${MALLOC_HEADER}\"\n")
list(APPEND SOURCES ${MALLOC_SOURCES} ${MALLOC_HEADER}
	${CMAKE_CURRENT_BINARY_DIR}/malloc-numa.h)


add_library(hpinuma_msource SHARED ${SOURCES})
target_link_libraries(hpinuma_msource
	PUBLIC
		hpinuma_base
		pthread
		numa
)
target_include_directories(hpinuma_msource ${include_dirs})
if (MALLOC_DEFINES)
	target_compile_definitions(hpinuma_msource ${MALLOC_DEFINES})
endif()

# Create public exports
set(HPINUMA_MSOURCE_EXPORT_HEADER "${PROJECT_BINARY_DIR}/src/PGASUS/msource/hpinuma_msource_export.h")
generate_export_header(hpinuma_msource EXPORT_FILE_NAME ${HPINUMA_MSOURCE_EXPORT_HEADER})
list(APPEND PUBLIC_HEADERS ${HPINUMA_MSOURCE_EXPORT_HEADER})
list(APPEND PROJECT_HEADERS)
# set_target_properties(hpinuma_msource PROPERTIES
#   PUBLIC_HEADER "${PUBLIC_HEADERS}")
addExportTarget(hpinuma_msource)
install(TARGETS hpinuma_msource
    EXPORT ${package_name}Targets COMPONENT dev
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
)
install(FILES ${HPINUMA_MSOURCE_EXPORT_HEADER}
	DESTINATION "${INSTALL_INCLUDE_DIR}/PGASUS/msource" COMPONENT dev)

add_sanitizers(hpinuma_msource)


if (PGASUS_BUILD_STATIC_LIBRARIES)
	add_library(hpinuma_msource_s STATIC ${SOURCES})
	target_link_libraries(hpinuma_msource_s
		PUBLIC
			hpinuma_base_s
			pthread
			numa
	)
	target_include_directories(hpinuma_msource_s ${include_dirs})
	if (MALLOC_DEFINES)
		target_compile_definitions(hpinuma_msource_s ${MALLOC_DEFINES})
	endif()
	target_compile_definitions(hpinuma_msource_s ${DEFAULT_COMPILE_DEFINITIONS}
		PUBLIC HPINUMA_MSOURCE_STATIC_DEFINE)

	# set_target_properties(hpinuma_msource_s PROPERTIES
	# 	PUBLIC_HEADER "${PUBLIC_HEADERS}")
	addExportTarget(hpinuma_msource_s)
	install(TARGETS hpinuma_msource_s
		EXPORT ${package_name}Targets COMPONENT dev
		ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT staticlib
	)
	add_sanitizers(hpinuma_msource_s)
endif()
