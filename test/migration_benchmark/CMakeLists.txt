
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
target_link_libraries(bench_migration PGASUS)
add_sanitizers(bench_migration)
cppcheck_target(bench_migration)

add_executable(bench_migration_s ${SOURCES})
target_link_libraries(bench_migration_s PGASUS_s)
add_sanitizers(bench_migration_s)
