
option(PGASUS_BUILD_DOCUMENTATION "Build the source documentation (requires doxygen)." ON)
mark_as_advanced(PGASUS_BUILD_DOCUMENTATION)

if (NOT PGASUS_BUILD_DOCUMENTATION)
    return()
endif()

add_subdirectory(doxygen)
