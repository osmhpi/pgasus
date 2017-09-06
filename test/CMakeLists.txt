
option(PGASUS_BUILD_TESTS "Build the tests and benchmarks (requires further 3rd party libs)." ON)
mark_as_advanced(PGASUS_BUILD_TESTS)

if (NOT PGASUS_BUILD_TESTS)
	return()
endif()

option(PGASUS_TEST_WITH_FAKE_TOPOLOGY
	"Simulate POWER-like virtualized topology by overriding some hwloc functions." OFF)
mark_as_advanced(PGASUS_TEST_WITH_FAKE_TOPOLOGY)
if (PGASUS_TEST_WITH_FAKE_TOPOLOGY)
	if (NOT PGASUS_BUILD_STATIC_LIBRARIES)
		message(WARNING "PGASUS_TEST_WITH_FAKE_TOPOLOGY requires PGASUS_BUILD_STATIC_LIBRARIES to be set.")
		set(PGASUS_TEST_WITH_FAKE_TOPOLOGY OFF)
	else()
		message("Replacing some hwloc calls by custom implementations.\n"
			"This simulates a NUMA topology with unevenly distributed CPUs "
			"and NUMA nodes without any CPU assigned.\n"
			"Note that tests will fail if there are not at least 4 CPUs "
			"(cores) available on the system.\n"
			"Warnings regarding read failures in /sys/devices/... can be ignored.")
	endif()
endif()

function(replace_by_static_libs LINK_LIST)
	set(result)
	set(target_libs hpinuma_util hpinuma_msource hpinuma)
	set(replacement_libs hpinuma_util_s hpinuma_msource_s hpinuma_s)
	foreach(lib ${${LINK_LIST}})
		list(FIND target_libs ${lib} idx)
		if (idx EQUAL -1)
			list(APPEND result ${lib})
		else()
			list(GET replacement_libs ${idx} repl_lib)
			list(APPEND result ${repl_lib})
		endif()
	endforeach()
	set(${LINK_LIST} ${result} PARENT_SCOPE)
endfunction()

if (PGASUS_TEST_WITH_FAKE_TOPOLOGY)
	set(lib_hpinuma_util hpinuma_util_s)
	set(lib_hpinuma_msource hpinuma_msource_s)
	set(lib_hpinuma hpinuma_s)
else()
	set(lib_hpinuma_util hpinuma_util)
	set(lib_hpinuma_msource hpinuma_msource)
	set(lib_hpinuma hpinuma)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include(CMakeParseArguments)

# Targets "tests" and "benchmarks"
# Build and run all available tests and benchmarks, respectively.
# Use the functions "add_test_without_ctest" and "add_benchmark" to add such
# targets.
add_custom_target(tests)
set_target_properties(tests PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
add_custom_target(benchmarks)
set_target_properties(benchmarks PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)

function(add_target_helper)
	set(options)
	set(oneValueArgs SUPER_TARGET_NAME NAME)
	set(multiValueArgs SOURCES LIBS PARAMS)
	cmake_parse_arguments("param"
		"${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	if (param_SOURCES)
		# inject fake hwloc functions if requested
		if (PGASUS_TEST_WITH_FAKE_TOPOLOGY)
			list(APPEND param_SOURCES fake_hwloc_topology.cpp)
		endif()
		add_executable(${param_NAME} ${param_SOURCES})
	elseif(NOT TARGET ${param_NAME})
		message(AUTHOR_WARNING "add_target_helper must either be supplied with \
target source files (paramter SOURCES) or called for an existing target.")
	endif()
	if (param_LIBS)
		# injecting fake hwloc functions requires static linkage
		if (PGASUS_TEST_WITH_FAKE_TOPOLOGY)
			replace_by_static_libs(param_LIBS)
		endif()
		target_link_libraries(${param_NAME} ${param_LIBS})
	endif()

	add_dependencies(${param_SUPER_TARGET_NAME} ${param_NAME})
	add_custom_command(TARGET ${param_SUPER_TARGET_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E echo "Starting: ${param_NAME} ${param_PARAMS}"
		COMMAND $<TARGET_FILE:${param_NAME}> ${param_PARAMS}
		COMMAND ${CMAKE_COMMAND} -E echo ""
		USES_TERMINAL
		VERBATIM)
	add_sanitizers(${param_NAME})
endfunction()
function(add_test_without_ctest)
	add_target_helper(SUPER_TARGET_NAME tests ${ARGV})
endfunction()
function(add_benchmark)
	add_target_helper(SUPER_TARGET_NAME benchmarks ${ARGV})
endfunction()


add_test_without_ctest(NAME test_msource SOURCES mspacetest.cpp
	LIBS hpinuma_msource)

# TODO implement more tests/benchmarks that work with MemSources only
if (NOT PGASUS_REPLACE_MALLOC)
	return()
endif()

add_test_without_ctest(NAME test_hashtable SOURCES hashtable_test.cpp
	LIBS hpinuma)

add_test_without_ctest(NAME test_malloc SOURCES malloctest.cpp LIBS hpinuma
	PARAMS 10 32 1200)

add_subdirectory(migration_benchmark)
# add_benchmark(NAME bench_migration)
# add_benchmark(NAME bench_migration_s)


# All other tests/benchmarks require the tasking module, which requires
# boost <= 1.60, which you have to build manually on some systems.
if (NOT PGASUS_WITH_TASKING)
	return()
endif()



add_benchmark(NAME bench_pagefault SOURCES bench_pagefault.cpp LIBS hpinuma
	PARAMS 100)

if (NOT PGASUS_PLATFORM_PPC64LE)
	add_benchmark(NAME bench_accessremote SOURCES bench_accessremote.cpp
		LIBS hpinuma
		PARAMS 10)
endif()

add_test_without_ctest(NAME test_tasking SOURCES tasking_test.cpp LIBS hpinuma
	PARAMS 1200 16)

add_test_without_ctest(NAME test_stdcontainers SOURCES test_stdcontainers.cpp
	LIBS hpinuma)

add_test_without_ctest(NAME test_mutex SOURCES mutextest.cpp LIBS hpinuma
	PARAMS 1200)

add_test_without_ctest(NAME test_prefault SOURCES test_prefault.cpp
	LIBS hpinuma
	PARAMS 2000)

add_subdirectory(hash_benchmark)
if (TARGET bench_hash)
	add_benchmark(NAME bench_hash
		PARAMS 1000 std)
	# add_benchmark(NAME bench_hash_s)
	add_sanitizers(bench_hash_s)
endif()
add_subdirectory(wordcount_benchmark)
set(wordcountBenchFile ${CMAKE_CURRENT_BINARY_DIR}/wordcount_bench_input.txt)
file(WRITE ${wordcountBenchFile} ${CMAKE_CURRENT_SOURCE_DIR}/../src/msource/msource.cpp)
if (TARGET bench_wc_omp)
	add_benchmark(NAME bench_wc_omp PARAMS ${wordcountBenchFile})
endif()
add_benchmark(NAME bench_wc_numa PARAMS ${wordcountBenchFile})
# add_benchmark(NAME bench_wc_numa_s)
add_sanitizers(bench_wc_numa_s)
add_subdirectory(bench_tasks)
add_benchmark(NAME bench_tasks_numa)
