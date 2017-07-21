#include <atomic>
#include <cstring>

#include "PGASUS/malloc.hpp"
#include "PGASUS/tasking/tasking.hpp"
#include "timer.hpp"

#include "test_helper.h"


float localAllocs(size_t nperthread) {
	Timer<int> timer(true);

	std::atomic_size_t threads(0);

	numa::wait(numa::forEachThread(numa::NodeList::allNodes(), [nperthread,&threads]() {
		std::vector<void*> allocs;
		allocs.reserve(0x1000);
		size_t id = threads.fetch_add(1);

		for (size_t i = 0; i < nperthread; i++) {
			allocs.push_back(malloc((i << 3) & 0x1FF));

			// clear from time to time
			if ((i & 0xFFF) == 0xFFF) {
				for (auto p : allocs) free(p);
				allocs.clear();
			}
		}
		for (auto p : allocs) free(p);
	}, 0));

	return (nperthread * threads.load()) / ((float)timer.stop_get() / 1000);
}

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "usage: %s local|remote elems\n", argv[0]);
		return 1;
	}

	testing::initialize();

	size_t elems = atoi(argv[2]);
	ASSERT_TRUE(elems > 0);

	if (!strcmp(argv[1], "local")) {
		float persec = localAllocs(elems);
		printf("%8.2f\n", persec);
	}

	return 0;
}
