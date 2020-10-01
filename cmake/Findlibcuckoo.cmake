#.rst:
# Findlibcuckoo
# --------
#
# Based on FindZLIB.cmake:
# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.
#
# Find the native libcuckoo includes and library.
#
# IMPORTED Targets
# ^^^^^^^^^^^^^^^^
#
# This module defines :prop_tgt:`IMPORTED` target ``libcuckoo::libcuckoo``, if
# libcuckoo has been found.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables:
#
# ::
#
#   libcuckoo_INCLUDE_DIRS   - where to find city.h, etc.
#   libcuckoo_LIBRARIES      - List of libraries when using libcuckoo.
#   libcuckoo_FOUND          - True if libcuckoo found.
#
# ::
#
# Hints
# ^^^^^
#
# A user may set ``libcuckoo_ROOT`` to a libcuckoo installation root to tell this
# module where to look.

set(_libcuckoo_SEARCHES)

# Search libcuckoo_ROOT first if it is set.
if(libcuckoo_ROOT)
  set(_libcuckoo_SEARCH_ROOT PATHS ${libcuckoo_ROOT} NO_DEFAULT_PATH)
  list(APPEND _libcuckoo_SEARCHES _libcuckoo_SEARCH_ROOT)
endif()

# Normal search.
set(_libcuckoo_SEARCH_NORMAL)
list(APPEND _libcuckoo_SEARCHES _libcuckoo_SEARCH_NORMAL)

set(libcuckoo_NAMES cityhash)
set(libcuckoo_NAMES_DEBUG cityhashd)

# Try each search configuration.
foreach(search ${_libcuckoo_SEARCHES})
  find_path(libcuckoo_INCLUDE_DIR NAMES city.h ${${search}} PATH_SUFFIXES include)
endforeach()

# Allow libcuckoo_LIBRARY to be set manually, as the location of the libcuckoo library
if(NOT libcuckoo_LIBRARY)
  foreach(search ${_libcuckoo_SEARCHES})
    find_library(libcuckoo_LIBRARY_RELEASE NAMES ${libcuckoo_NAMES} ${${search}} PATH_SUFFIXES lib)
    find_library(libcuckoo_LIBRARY_DEBUG NAMES ${libcuckoo_NAMES_DEBUG} ${${search}} PATH_SUFFIXES lib)
  endforeach()

  include(SelectLibraryConfigurations)
  select_library_configurations(libcuckoo)
endif()

unset(libcuckoo_NAMES)
unset(libcuckoo_NAMES_DEBUG)

mark_as_advanced(libcuckoo_INCLUDE_DIR)


# handle the QUIETLY and REQUIRED arguments and set libcuckoo_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(libcuckoo REQUIRED_VARS libcuckoo_LIBRARY libcuckoo_INCLUDE_DIR)

if(libcuckoo_FOUND)
    set(libcuckoo_INCLUDE_DIRS ${libcuckoo_INCLUDE_DIR})

    if(NOT libcuckoo_LIBRARIES)
      set(libcuckoo_LIBRARIES ${libcuckoo_LIBRARY})
    endif()

    if(NOT TARGET libcuckoo::libcuckoo)
      add_library(libcuckoo::libcuckoo UNKNOWN IMPORTED)
      set_target_properties(libcuckoo::libcuckoo PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${libcuckoo_INCLUDE_DIRS}")

      if(libcuckoo_LIBRARY_RELEASE)
        set_property(TARGET libcuckoo::libcuckoo APPEND PROPERTY
          IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(libcuckoo::libcuckoo PROPERTIES
          IMPORTED_LOCATION_RELEASE "${libcuckoo_LIBRARY_RELEASE}")
      endif()

      if(libcuckoo_LIBRARY_DEBUG)
        set_property(TARGET libcuckoo::libcuckoo APPEND PROPERTY
          IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(libcuckoo::libcuckoo PROPERTIES
          IMPORTED_LOCATION_DEBUG "${libcuckoo_LIBRARY_DEBUG}")
      endif()

      if(NOT libcuckoo_LIBRARY_RELEASE AND NOT libcuckoo_LIBRARY_DEBUG)
        set_property(TARGET libcuckoo::libcuckoo APPEND PROPERTY
          IMPORTED_LOCATION "${libcuckoo_LIBRARY}")
      endif()
    endif()
endif()
