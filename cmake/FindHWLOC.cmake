#.rst:
# FindHWLOC
# --------
#
# Based on FindZLIB.cmake:
# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.
#
# Find the native HWLOC includes and library.
#
# IMPORTED Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines :prop_tgt:`IMPORTED` target ``hwloc``, if
# HWLOC has been found.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables:
#
# ::
#
#   HWLOC_INCLUDE_DIRS   - where to find hwloc.h, etc.
#   HWLOC_LIBRARIES      - List of libraries when using hwloc.
#   HWLOC_FOUND          - True if HWLOC found.
#
# ::
#
# Hints
# ^^^^^
#
# A user may set ``HWLOC_ROOT`` to a HWLOC installation root to tell this
# module where to look.

set(_HWLOC_SEARCHES)

# Search HWLOC_ROOT first if it is set.
if(HWLOC_ROOT)
  set(_HWLOC_SEARCH_ROOT PATHS ${HWLOC_ROOT} NO_DEFAULT_PATH)
  list(APPEND _HWLOC_SEARCHES _HWLOC_SEARCH_ROOT)
endif()

# Normal search.
set(_HWLOC_SEARCH_NORMAL)
list(APPEND _HWLOC_SEARCHES _HWLOC_SEARCH_NORMAL)

set(HWLOC_NAMES hwloc hwloc1)
set(HWLOC_NAMES_DEBUG hwlocd hwlocd1)

# Try each search configuration.
foreach(search ${_HWLOC_SEARCHES})
  find_path(HWLOC_INCLUDE_DIR NAMES hwloc.h ${${search}} PATH_SUFFIXES include)
endforeach()

# Allow HWLOC_LIBRARY to be set manually, as the location of the HWLOC library
if(NOT HWLOC_LIBRARY)
  foreach(search ${_HWLOC_SEARCHES})
    find_library(HWLOC_LIBRARY_RELEASE NAMES ${HWLOC_NAMES} ${${search}} PATH_SUFFIXES lib)
    find_library(HWLOC_LIBRARY_DEBUG NAMES ${HWLOC_NAMES_DEBUG} ${${search}} PATH_SUFFIXES lib)
  endforeach()

  include(SelectLibraryConfigurations)
  select_library_configurations(HWLOC)
endif()

unset(HWLOC_NAMES)
unset(HWLOC_NAMES_DEBUG)

mark_as_advanced(HWLOC_INCLUDE_DIR)


# handle the QUIETLY and REQUIRED arguments and set HWLOC_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(HWLOC REQUIRED_VARS HWLOC_LIBRARY HWLOC_INCLUDE_DIR)

if(HWLOC_FOUND)
    set(HWLOC_INCLUDE_DIRS ${HWLOC_INCLUDE_DIR})

    if(NOT HWLOC_LIBRARIES)
      set(HWLOC_LIBRARIES ${HWLOC_LIBRARY})
    endif()

    if(NOT TARGET hwloc)
      add_library(hwloc UNKNOWN IMPORTED)
      set_target_properties(hwloc PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${HWLOC_INCLUDE_DIRS}")

      if(HWLOC_LIBRARY_RELEASE)
        set_property(TARGET hwloc APPEND PROPERTY
          IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(hwloc PROPERTIES
          IMPORTED_LOCATION_RELEASE "${HWLOC_LIBRARY_RELEASE}")
      endif()

      if(HWLOC_LIBRARY_DEBUG)
        set_property(TARGET hwloc APPEND PROPERTY
          IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(hwloc PROPERTIES
          IMPORTED_LOCATION_DEBUG "${HWLOC_LIBRARY_DEBUG}")
      endif()

      if(NOT HWLOC_LIBRARY_RELEASE AND NOT HWLOC_LIBRARY_DEBUG)
        set_property(TARGET hwloc APPEND PROPERTY
          IMPORTED_LOCATION "${HWLOC_LIBRARY}")
      endif()
    endif()
endif()
