
include_directories(.)
include_directories(../..)

set(SOURCES
	test.hpp
	rand.hpp
	main.cpp
)

find_package(TBB)
if (NOT TBB_FOUND)
	message("TBB not found, skipping hash benchmark.")
	return()
endif()

find_package(libcuckoo)
if (NOT libcuckoo_FOUND)
	message("libcuckoo not found, skipping hash benchmark.")
	return()
endif()

include_directories(
	${TBB_INCLUDE_DIRS}
	${libcuckoo_INCLUDE_DIRS}
)

add_executable(bench_hash ${SOURCES})
target_link_libraries(bench_hash hpinuma ${TBB_LIBRARIES})

add_executable(bench_hash_s ${SOURCES})
target_link_libraries(bench_hash_s hpinuma_s ${TBB_LIBRARIES})
