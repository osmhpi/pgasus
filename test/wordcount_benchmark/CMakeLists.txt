
option(PGASUS_BENCH_WORDCOUNT_WITH_TCMALLOC
	"Add Wordcount benchmark variant that uses TCMalloc. Required gperftools."
	OFF)
mark_as_advanced(PGASUS_BENCH_WORDCOUNT_WITH_TCMALLOC)

set(SRC_UTIL
	buffer.hpp
	buffer.cpp
	exec.hpp
	util.hpp
	util.cpp
	gzstream.cpp
	gzstream.h
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

find_package(OpenMP QUIET)

if (OPENMP_FOUND)
	add_executable(bench_wc_omp main.cpp exec_openmp.cpp ${SRC_UTIL})
	set_target_properties(bench_wc_omp PROPERTIES COMPILE_FLAGS ${OpenMP_CXX_FLAGS})
	set_target_properties(bench_wc_omp PROPERTIES LINK_FLAGS ${OpenMP_CXX_FLAGS})
	target_link_libraries(bench_wc_omp ${ZLIB_LIBRARIES})
else()
	message("OpenMP not found. Skipping bench_wc_omp.")
endif()

add_executable(bench_wc_numa main.cpp exec_numa.cpp ${SRC_UTIL})
target_link_libraries(bench_wc_numa hpinuma ${ZLIB_LIBRARIES})
cppcheck_target(bench_wc_numa)

add_executable(bench_wc_numa_s main.cpp exec_numa.cpp ${SRC_UTIL})
target_link_libraries(bench_wc_numa_s hpinuma_s ${ZLIB_LIBRARIES})

if (OPENMP_FOUND)
	add_executable(bench_wc_pgasomp main.cpp exec_pgasomp.cpp ${SRC_UTIL})
	target_link_libraries(bench_wc_pgasomp hpinuma_msource ${ZLIB_LIBRARIES})
	set_target_properties(bench_wc_pgasomp PROPERTIES COMPILE_FLAGS ${OpenMP_CXX_FLAGS})
	set_target_properties(bench_wc_pgasomp PROPERTIES LINK_FLAGS ${OpenMP_CXX_FLAGS})

	add_executable(bench_wc_pgasomp_s main.cpp exec_pgasomp.cpp ${SRC_UTIL})
	target_link_libraries(bench_wc_pgasomp_s hpinuma_msource_s ${ZLIB_LIBRARIES})
	set_target_properties(bench_wc_pgasomp_s PROPERTIES COMPILE_FLAGS ${OpenMP_CXX_FLAGS})
	set_target_properties(bench_wc_pgasomp_s PROPERTIES LINK_FLAGS ${OpenMP_CXX_FLAGS})
endif()

if (PGASUS_BENCH_WORDCOUNT_WITH_TCMALLOC)
	find_library(tcmalloc_minimal_PATH
		NAMES tcmalloc_minimal tcmalloc
		DOC "Location of tcmalloc library that is part of gperftools."
	)
	if (NOT tcmalloc_minimal_PATH)
		message(WARNING "tcmalloc library not found. Skipping wordcount/TCMalloc target.")
	else()
		add_executable(bench_wc_omp_TCMalloc main.cpp exec_openmp.cpp ${SRC_UTIL})
		set_target_properties(bench_wc_omp_TCMalloc PROPERTIES
			COMPILE_FLAGS ${OpenMP_CXX_FLAGS}
			LINK_FLAGS ${OpenMP_CXX_FLAGS}
		)
		target_link_libraries(bench_wc_omp_TCMalloc
			${tcmalloc_minimal_PATH}
			${ZLIB_LIBRARIES}
			-fno-builtin-malloc
			-fno-builtin-calloc
			-fno-builtin-realloc
			-fno-builtin-free
		)
	endif()
endif()
