cmake_minimum_required(VERSION 2.6)

include_directories(.)
include_directories(../..)

add_executable(bench_tasks_numa bench_tasks.cpp)
target_link_libraries(bench_tasks_numa hpinuma)
