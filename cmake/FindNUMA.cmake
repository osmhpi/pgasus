#.rst:
# FindNUMA
# --------
#
# Based on FindZLIB.cmake:
# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.
#
# Find the native numa includes and library.
#
# IMPORTED Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines :prop_tgt:`IMPORTED` target ``numa``, if
# NUMA has been found.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables:
#
# ::
#
#   NUMA_INCLUDE_DIRS   - where to find numa.h, etc.
#   NUMA_LIBRARIES      - List of libraries when using numa.
#   NUMA_FOUND          - True if NUMA found.
#
# ::
#
# Hints
# ^^^^^
#
# A user may set ``NUMA_ROOT`` to a NUMA installation root to tell this
# module where to look.

set(_NUMA_SEARCHES)

# Search NUMA_ROOT first if it is set.
if(NUMA_ROOT)
  set(_NUMA_SEARCH_ROOT PATHS ${NUMA_ROOT} NO_DEFAULT_PATH)
  list(APPEND _NUMA_SEARCHES _NUMA_SEARCH_ROOT)
endif()

# Normal search.
set(_NUMA_SEARCH_NORMAL)
list(APPEND _NUMA_SEARCHES _NUMA_SEARCH_NORMAL)

set(NUMA_NAMES numa numa1)
set(NUMA_NAMES_DEBUG numad numad1)

# Try each search configuration.
foreach(search ${_NUMA_SEARCHES})
  find_path(NUMA_INCLUDE_DIR NAMES numa.h ${${search}} PATH_SUFFIXES include)
endforeach()

# Allow NUMA_LIBRARY to be set manually, as the location of the NUMA library
if(NOT NUMA_LIBRARY)
  foreach(search ${_NUMA_SEARCHES})
    find_library(NUMA_LIBRARY_RELEASE NAMES ${NUMA_NAMES} ${${search}} PATH_SUFFIXES lib)
    find_library(NUMA_LIBRARY_DEBUG NAMES ${NUMA_NAMES_DEBUG} ${${search}} PATH_SUFFIXES lib)
  endforeach()

  include(SelectLibraryConfigurations)
  select_library_configurations(NUMA)
endif()

unset(NUMA_NAMES)
unset(NUMA_NAMES_DEBUG)

mark_as_advanced(NUMA_INCLUDE_DIR)


# handle the QUIETLY and REQUIRED arguments and set NUMA_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(NUMA REQUIRED_VARS NUMA_LIBRARY NUMA_INCLUDE_DIR)

if(NUMA_FOUND)
    set(NUMA_INCLUDE_DIRS ${NUMA_INCLUDE_DIR})

    if(NOT NUMA_LIBRARIES)
      set(NUMA_LIBRARIES ${NUMA_LIBRARY})
    endif()

    if(NOT TARGET numa)
      add_library(numa UNKNOWN IMPORTED)
      set_target_properties(numa PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${NUMA_INCLUDE_DIRS}")

      if(NUMA_LIBRARY_RELEASE)
        set_property(TARGET numa APPEND PROPERTY
          IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(numa PROPERTIES
          IMPORTED_LOCATION_RELEASE "${NUMA_LIBRARY_RELEASE}")
      endif()

      if(NUMA_LIBRARY_DEBUG)
        set_property(TARGET numa APPEND PROPERTY
          IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(numa PROPERTIES
          IMPORTED_LOCATION_DEBUG "${NUMA_LIBRARY_DEBUG}")
      endif()

      if(NOT NUMA_LIBRARY_RELEASE AND NOT NUMA_LIBRARY_DEBUG)
        set_property(TARGET numa APPEND PROPERTY
          IMPORTED_LOCATION "${NUMA_LIBRARY}")
      endif()
    endif()
endif()
