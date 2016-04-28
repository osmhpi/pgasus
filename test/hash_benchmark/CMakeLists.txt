cmake_minimum_required(VERSION 2.6)

include_directories(.)
include_directories(../..)

set(SOURCES
	test.hpp
	rand.hpp
	main.cpp
)

add_executable(bench_hash ${SOURCES})
target_link_libraries(bench_hash hpinuma tbb)

add_executable(bench_hash_s ${SOURCES})
target_link_libraries(bench_hash_s hpinuma_s tbb)
