
option(OPTION_BUILD_DOCUMENTATION "Build the source documentation (requires doxygen)." ON)
mark_as_advanced(OPTION_BUILD_DOCUMENTATION)

if (NOT OPTION_BUILD_DOCUMENTATION)
    return()
endif()

add_subdirectory(doxygen)
