
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
	target_link_libraries(bench_wc_omp hpinuma ${ZLIB_LIBRARIES})
else()
	message("OpenMP not found. Skipping bench_wc_omp.")
endif()

add_executable(bench_wc_numa main.cpp exec_numa.cpp ${SRC_UTIL})
target_link_libraries(bench_wc_numa hpinuma ${ZLIB_LIBRARIES})

add_executable(bench_wc_numa_s main.cpp exec_numa.cpp ${SRC_UTIL})
target_link_libraries(bench_wc_numa_s hpinuma_s ${ZLIB_LIBRARIES})
