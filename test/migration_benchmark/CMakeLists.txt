cmake_minimum_required(VERSION 2.6)

include_directories(.)
include_directories(../..)

set(SOURCES
	executor.hpp
	executor_movepages.hpp
	executor_copy.hpp
	executor_msource.hpp
	nodehelper.cpp
	nodehelper.h
	main.cpp
)

add_executable(bench_migration ${SOURCES})
target_link_libraries(bench_migration hpinuma)

add_executable(bench_migration_s ${SOURCES})
target_link_libraries(bench_migration_s hpinuma_s)
