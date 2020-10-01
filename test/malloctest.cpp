#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstdlib>
#include <vector>

#include <semaphore.h>

#include "PGASUS/base/topology.hpp"
#include "../src/base/strutil.hpp"

#include "test_helper.h"


static bool get_elements_from_string(const std::string &s, std::vector<int> &ret) {
	for (auto &part : numa::util::split(s, ',')) {
		auto nums = numa::util::split(part, '-');

		int a = -1, b = -1;
		if (nums.size() < 1 || sscanf(nums[0].c_str(), "%d", &a) == 0) {
			fprintf(stderr, "Malformed range string: '%s' from '%s'\n",
				part.c_str(), s.c_str());
			return false;
		}
		if (nums.size() == 2) {
			if (sscanf(nums[1].c_str(), "%d", &b) == 0) {
				fprintf(stderr, "Malformed range string: '%s' from '%s'\n",
					part.c_str(), s.c_str());
				return false;
			}
		} else {
			b = a;
		}

		for (int i = a; i <= b; i++)
			ret.push_back(i);
	}

	return true;
}

static int blocks;
static int nthreads;
static int nnodes;
static sem_t sem;

static void* thfun(void *data) {
	ASSERT_TRUE(sem_wait(&sem) == 0);
	
	std::vector<void*> allocs;
	allocs.reserve(blocks);

	printf("%zu/%d\n", reinterpret_cast<size_t>(data), nthreads);

	for (int i = 0; i < blocks; i++) {
		allocs.push_back(malloc((i << 3) & 0x1FF));
		
		// clear from time to time
		if ((i & 0xFFF) == 0xFFF) {
			for (auto p : allocs) free(p);
			allocs.clear();
		}
	}
	for (auto p : allocs) free(p);

	return nullptr;
}

int main (int argc, char const* argv[])
{
	testing::initialize();

	if (argc < 4) {
		printf("Usage: %s nodes threads blocks\n", argv[0]);
		return 0;
	}

	std::vector<int> nodes;
	if (!get_elements_from_string(argv[1], nodes)) {
		return 0;
	}
	nthreads = atoi(argv[2]);
	blocks = atoi(argv[3]);
	nnodes = nodes.size();

	const numa::util::Topology *topo = numa::util::Topology::get();
	std::vector<pthread_t> threads;
	
	ASSERT_TRUE(sem_init(&sem, 0, 0) == 0);
	
	printf("start\n");

	// start threads
	for (int i = 0; i < int(nodes.size()); i++) {
		// nodemask
		cpu_set_t cpu_set;
		CPU_ZERO(&cpu_set);
		for (int cpu : topo->get_node(i)->cpus)
			CPU_SET(cpu, &cpu_set);

		pthread_attr_t attr;
		ASSERT_TRUE(pthread_attr_init(&attr) == 0);
		ASSERT_TRUE(pthread_attr_setaffinity_np(&attr, sizeof(cpu_set), &cpu_set) == 0);

		for (int k = 0; k < nthreads; k++) {
			pthread_t th;
			ASSERT_TRUE(pthread_create(&th, &attr, thfun, reinterpret_cast<void*>(k)) == 0);
			threads.push_back(th);
		}
		ASSERT_TRUE(pthread_attr_destroy(&attr) == 0);
	}

	// go
	printf("run threads\n");
	for (size_t i = 0; i < threads.size(); ++i) sem_post(&sem);

	for (pthread_t &th : threads)
		ASSERT_TRUE(pthread_join(th, nullptr) == 0);
	
	printf("done\n");
	
	return 0;
}
