
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

add_executable(bench_hash ${SOURCES})
target_link_libraries(bench_hash
	PUBLIC
		hpinuma
		${TBB_LIBRARIES}
)
target_include_directories(bench_hash
	PUBLIC
		${libcuckoo_INCLUDE_DIRS}
)

add_executable(bench_hash_s ${SOURCES})
target_link_libraries(bench_hash_s
	PUBLIC
		hpinuma_s
		${TBB_LIBRARIES}
)
target_include_directories(bench_hash_s
	PUBLIC
		${libcuckoo_INCLUDE_DIRS}
)
