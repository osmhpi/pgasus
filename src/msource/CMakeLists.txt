
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


add_library(PGASUS_msource SHARED ${SOURCES})
target_link_libraries(PGASUS_msource
	PUBLIC
		PGASUS_base
		pthread
		numa
)
target_include_directories(PGASUS_msource ${include_dirs})
if (MALLOC_DEFINES)
	target_compile_definitions(PGASUS_msource ${MALLOC_DEFINES})
endif()
cppcheck_target(PGASUS_msource)

# Create public exports
set(PGASUS_MSOURCE_EXPORT_HEADER "${PROJECT_BINARY_DIR}/src/PGASUS/msource/PGASUS_msource_export.h")
generate_export_header(PGASUS_msource EXPORT_FILE_NAME ${PGASUS_MSOURCE_EXPORT_HEADER})
list(APPEND PUBLIC_HEADERS ${PGASUS_MSOURCE_EXPORT_HEADER})
list(APPEND PROJECT_HEADERS)
# set_target_properties(PGASUS_msource PROPERTIES
#   PUBLIC_HEADER "${PUBLIC_HEADERS}")
addExportTarget(PGASUS_msource)
install(TARGETS PGASUS_msource
    EXPORT ${package_name}Targets COMPONENT dev
    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
)
install(FILES ${PGASUS_MSOURCE_EXPORT_HEADER}
	DESTINATION "${INSTALL_INCLUDE_DIR}/PGASUS/msource" COMPONENT dev)

add_sanitizers(PGASUS_msource)


if (PGASUS_BUILD_STATIC_LIBRARIES)
	add_library(PGASUS_msource_s STATIC ${SOURCES})
	target_link_libraries(PGASUS_msource_s
		PUBLIC
			PGASUS_base_s
			pthread
			numa
	)
	target_include_directories(PGASUS_msource_s ${include_dirs})
	if (MALLOC_DEFINES)
		target_compile_definitions(PGASUS_msource_s ${MALLOC_DEFINES})
	endif()
	target_compile_definitions(PGASUS_msource_s ${DEFAULT_COMPILE_DEFINITIONS}
		PUBLIC HPINUMA_MSOURCE_STATIC_DEFINE)

	# set_target_properties(PGASUS_msource_s PROPERTIES
	# 	PUBLIC_HEADER "${PUBLIC_HEADERS}")
	addExportTarget(PGASUS_msource_s)
	install(TARGETS PGASUS_msource_s
		EXPORT ${package_name}Targets COMPONENT dev
		ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT staticlib
	)
	add_sanitizers(PGASUS_msource_s)
endif()
