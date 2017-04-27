
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_msource mspacetest.cpp)
target_link_libraries(test_msource hpinuma_msource)

add_executable(bench_pagefault bench_pagefault.cpp)
target_link_libraries(bench_pagefault hpinuma)

add_executable(bench_accessremote bench_accessremote.cpp)
target_link_libraries(bench_accessremote hpinuma)

add_executable(test_tasking tasking_test.cpp)
target_link_libraries(test_tasking hpinuma)

add_executable(test_mutex mutextest.cpp)
target_link_libraries(test_mutex hpinuma)

add_executable(test_malloc malloctest.cpp)
target_link_libraries(test_malloc hpinuma)

add_executable(test_stdcontainers test_stdcontainers.cpp)
target_link_libraries(test_stdcontainers hpinuma)

add_executable(test_hashtable hashtable_test.cpp)
target_link_libraries(test_hashtable hpinuma)

add_executable(test_prefault test_prefault.cpp)
target_link_libraries(test_prefault hpinuma)

add_subdirectory(migration_benchmark)
add_subdirectory(hash_benchmark)
add_subdirectory(wordcount_benchmark)
add_subdirectory(bench_tasks)
