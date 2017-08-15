cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
if (POLICY CMP0063)
	cmake_policy(SET CMP0063 NEW)
endif()

include(GenerateExportHeader)


project(LibNumaCpp VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_MODULE_PATH
	"${PROJECT_SOURCE_DIR}/cmake"
	"${PROJECT_SOURCE_DIR}/3rdParty/sanitizers-cmake/cmake"
	${CMAKE_MODULE_PATH}
)

macro(add_PGASUS_option NAME DOC INITVALUE)
	option(PGASUS_${NAME} ${DOC} ${INITVALUE})
	if (PGASUS_${NAME})
		set(${NAME} 1)
	else()
		set(${NAME} 0)
	endif()
endmacro()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(LINUX 1)
endif()


set(PGASUS_PLATFORM_X86_64 0)
set(PGASUS_PLATFORM_PPC64LE 0)
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	set(PGASUS_PLATFORM_X86_64 1)
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "ppc64le")
	set(PGASUS_PLATFORM_PPC64LE 1)
else()
	message(WARNING "Unsupported platform: \"${CMAKE_SYSTEM_PROCESSOR}\"")
endif()


set(MEM_SOURCE_USE_PTHREAD_SPINLOCK_doc
	"Use pthread spinlocks instead of own implementation")
set(MEM_SOURCE_USE_PTHREAD_SPINLOCK_default OFF)
if (PGASUS_PLATFORM_PPC64LE)
	set(MEM_SOURCE_USE_PTHREAD_SPINLOCK_default ON)
endif()
option(PGASUS_PROFILING_GPROF "Compile for profiling with gprof" OFF)
add_PGASUS_option(ENABLE_DEBUG_LOG "Enable debug log for PGASUS/NUMA operations" OFF)
if (PGASUS_PLATFORM_PPC64LE AND PGASUS_ENABLE_DEBUG_LOG)
	message(WARNING "PGASUS_ENABLE_DEBUG_LOG is set, but most performance logs "
		"are not implemented for POWER platforms.")
endif()
add_PGASUS_option(MEM_SOURCE_USE_PTHREAD_SPINLOCK
	${MEM_SOURCE_USE_PTHREAD_SPINLOCK_doc} ${MEM_SOURCE_USE_PTHREAD_SPINLOCK_default})
if (PGASUS_PLATFORM_PPC64LE AND NOT PGASUS_MEM_SOURCE_USE_PTHREAD_SPINLOCK)
	message("Spinlocks are not implemented on your platform. "
		"Using pthread spinlocks instead (see PGASUS_MEM_SOURCE_USE_PTHREAD_SPINLOCK).")
	set(PGASUS_MEM_SOURCE_USE_PTHREAD_SPINLOCK ON CACHE BOOL
		${MEM_SOURCE_USE_PTHREAD_SPINLOCK_doc} FORCE)
endif()

add_PGASUS_option(WITH_TASKING
	"Include tasking support, currently requiring boost version >= 1.58 and <= 1.60." ON)


if (NOT DEFAULT_COMPILE_DEFINITIONS)
	set(DEFAULT_COMPILE_DEFINITIONS)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	add_compile_options(
		-Wpedantic
		-Wall
		-Wno-missing-braces
	)
	if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.8)
		set(OPTION_CLANG_DEBUGGER gdb CACHE STRING
			"Debugger for that clang optimizes the DWARF debugging information")
		set_property(CACHE OPTION_CLANG_DEBUGGER PROPERTY STRINGS gdb lldb)
		add_compile_options($<$<CONFIG:Debug>:-g${OPTION_CLANG_DEBUGGER}>)
	endif()
	if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.0")
		add_compile_options(
			$<$<CONFIG:Debug>:-fno-limit-debug-info>
		)
	endif()

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options(
		-Wpedantic
		-Wall

		$<$<CONFIG:Debug>:-ggdb3>
	)

	if (NOT PGASUS_PLATFORM_PPC64LE)
		add_compile_options(
			-march=native
		)
	endif()

	if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.0")
		list(APPEND DEFAULT_COMPILE_DEFINITIONS
			PUBLIC _GLIBCXX_USE_CXX11_ABI=1)
	endif()

	if (PGASUS_PROFILING_GPROF)
		add_compile_options(-g -pg)
		link_libraries(-g -pg)
	endif()

	option(PGASUS_USE_GLIBCXX_DEBUG
		"Enable libstdc++ debug mode in Debug builds (Define _GLIBCXX_DEBUG - container bounds checks)." OFF)
	if (PGASUS_USE_GLIBCXX_DEBUG)
		list(APPEND DEFAULT_COMPILE_DEFINITIONS $<$<CONFIG:Debug>:PUBLIC _GLIBCXX_DEBUG>)
	endif()

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
	add_compile_options(-Wall $<$<CONFIG:Release>:-Ofast -no-prec-div -xHOST>)

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "XL") # IBM XLC
	add_compile_options(
		# CMake does not know to configure XLC for C++11
		$<$<COMPILE_LANGUAGE:CXX>:-std=c++11>
		$<$<COMPILE_LANGUAGE:C>:-std=c11>
	)
endif()


option(PGASUS_BUILD_STATIC_LIBRARIES "Also build static libraries" ON)

option(PGASUS_RDTSC_PROFILING "Also rdtsc()-based profiling for msources and spinlocks" OFF)
if (PGASUS_RDTSC_PROFILING)
	set(NUMA_PROFILE_SPINLOCK 1)
	set(NUMA_PROFILE_MSOURCE 1)
else()
	set(NUMA_PROFILE_SPINLOCK 0)
	set(NUMA_PROFILE_MSOURCE 0)
endif()

option(PGASUS_DEBUG_MALLOC "Annotate freshly allocated and freed memory regions with special values (0xAA, 0xBB, 0xCC)" OFF)
if (PGASUS_DEBUG_MALLOC)
	set(MEM_SOURCE_FILL_MEMORY_DEBUG 1)
else()
	set(MEM_SOURCE_FILL_MEMORY_DEBUG 0)
endif()


# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (PGASUS_WITH_TASKING)
	set(BOOST_MIN_REQUIRED_VERSION 1.56)
	set(BOOST_MAX_REQUIRED_VERSION 1.60)
	set(BOOST_REQUIRED_COMPONENTS context chrono thread system)
	find_package(Boost ${BOOST_MIN_REQUIRED_VERSION} COMPONENTS ${BOOST_REQUIRED_COMPONENTS} REQUIRED)
	set(BOOST_FOUND_VERSION ${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION})
	if (BOOST_FOUND_VERSION VERSION_GREATER BOOST_MAX_REQUIRED_VERSION)
		message(FATAL_ERROR "Boost version newer than ${BOOST_MAX_REQUIRED_VERSION} is currently not supported. \
Disable tasking support to omit the Boost dependency (See PGASUS_WITH_TASKING).")
	endif()
endif()
find_package(ZLIB REQUIRED)
find_package(NUMA REQUIRED)
find_package(HWLOC REQUIRED)
find_package(Sanitizers QUIET)
if (NOT Sanitizers_FOUND)
	function(add_sanitizers target)
	endfunction()
endif()


set(package_name "PGASUS")

set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")
if(WIN32 AND NOT CYGWIN)
	set(DEF_INSTALL_CMAKE_DIR CMake)
else()
	set(DEF_INSTALL_CMAKE_DIR lib/CMake/${package_name})
endif()
set(INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH
	"Installation directory for CMake files")

# Make relative paths absolute (needed later on)
foreach(p LIB BIN INCLUDE CMAKE)
	set(var INSTALL_${p}_DIR)
	if(NOT IS_ABSOLUTE "${${var}}")
		set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
	endif()
endforeach()

# populate ;-separated list with targets to be exported
set(export_targets_bin "" CACHE INTERNAL "")
set(export_targets_shlib "" CACHE INTERNAL "")
set(export_targets_staticlib "" CACHE INTERNAL "")
function(addExportTarget TARGET_NAME)
	get_target_property(type ${TARGET_NAME} TYPE)
	set(targetVar)
	if (type STREQUAL EXECUTABLE)
		set(targetVar export_targets_bin)
	elseif(type STREQUAL SHARED_LIBRARY)
		set(targetVar export_targets_shlib)
	elseif(type STREQUAL STATIC_LIBRARY)
		set(targetVar export_targets_staticlib)
	else()
		message(FATAL_ERROR "Unexpected target type: ${type} (target: \"${TARGET_NAME}\")")
	endif()
	set(separator)
	if (${targetVar})
		set(separator ";")
	endif()
	set(${targetVar} "${${targetVar}}${separator}${TARGET_NAME}"
		CACHE INTERNAL "")
endfunction()


set(PGASUS_CONFIG_FILE ${PROJECT_BINARY_DIR}/src/PGASUS/PGASUS-config.h)
configure_file(${PROJECT_SOURCE_DIR}/cmake/PGASUS-config.h.in ${PGASUS_CONFIG_FILE})
# Install the whole include/PGASUS/... tree, which are the public headers of the libraries
# NOTE: Installation of only a subset of the libraries is not possible that way!
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/PGASUS"
	DESTINATION ${INSTALL_INCLUDE_DIR} COMPONENT dev)
install(FILES ${PGASUS_CONFIG_FILE}
	DESTINATION ${INSTALL_INCLUDE_DIR}/PGASUS COMPONENT dev)

# Set postfix for debug versions of libraries. This does not apply to executables!
set(CMAKE_DEBUG_POSTFIX "_d")


# add sub-projects
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(docs)


# Export to be usable in external projects
set(export_targets ${export_targets_bin} ${export_targets_shlib} ${export_targets_staticlib})
export(TARGETS ${export_targets}
		NAMESPACE ${package_name}::
		FILE "${PROJECT_BINARY_DIR}/${package_name}Targets.cmake")
export(PACKAGE ${package_name})

# Create the ${package_name}Config.cmake and ${package_name}ConfigVersion files
file(RELATIVE_PATH REL_INCLUDE_DIR "${INSTALL_CMAKE_DIR}"
	 "${INSTALL_INCLUDE_DIR}")
# ... for the build tree
set(CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}")
configure_file("cmake/${package_name}Config.cmake.in"
	"${PROJECT_BINARY_DIR}/${package_name}Config.cmake" @ONLY)
# ... for the install tree
set(CONF_INCLUDE_DIRS "\${${package_name}_CMAKE_DIR}/${REL_INCLUDE_DIR}")
configure_file("cmake/${package_name}Config.cmake.in"
	"${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${package_name}Config.cmake" @ONLY)
# ... for both
configure_file("cmake/${package_name}ConfigVersion.cmake.in"
	"${PROJECT_BINARY_DIR}/${package_name}ConfigVersion.cmake" @ONLY)

# Install the ${package_name}Config.cmake and ${package_name}ConfigVersion.cmake
install(FILES
	"${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${package_name}Config.cmake"
	"${PROJECT_BINARY_DIR}/${package_name}ConfigVersion.cmake"
	DESTINATION "${INSTALL_CMAKE_DIR}" COMPONENT dev)

# Install the export set for use with the install-tree
install(EXPORT ${package_name}Targets
		NAMESPACE ${package_name}::
		DESTINATION "${INSTALL_CMAKE_DIR}"
		COMPONENT dev)
