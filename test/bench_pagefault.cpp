#include <sys/mman.h>    /* for mmap */
#include <numaif.h>      /* for mbind */
#include <numa.h>

#include <cstdint>
#include <cassert>
#include <vector>
#include <cstdio>
#include <algorithm>

#include "tasking/tasking.hpp"
#include "msource/node_replicated.hpp"
#include "timer.hpp"

/**
 * Bind memory region to given NUMA node
 */
int bindMemory(void *p, size_t sz, int node) {
	int ret;
	unsigned mflags = MPOL_MF_STRICT | MPOL_MF_MOVE;

	// node bit mask
	static constexpr size_t NODE_MASK_MAX = 1024;
	static constexpr size_t ITEM_SIZE = 8*sizeof(unsigned long);
	static constexpr size_t NODE_MASK_ITEMS = NODE_MASK_MAX / ITEM_SIZE;
	static_assert(NODE_MASK_ITEMS * ITEM_SIZE == NODE_MASK_MAX, "Invalid node mask size");

	assert(node < (int)NODE_MASK_MAX);

	// init bit mask - make sure to do no dynamic memory allocation here,
	// so store the bitmask on the stack
	unsigned long nodemask[NODE_MASK_ITEMS];
	memset(nodemask, 0, NODE_MASK_ITEMS * sizeof(unsigned long));
	nodemask[node/ITEM_SIZE] |= (1LL << (node%ITEM_SIZE));

	ret = mbind(p, sz, MPOL_BIND, nodemask, NODE_MASK_MAX, mflags);

	return ret;
}

/**
 * Allocate sz bytes from system. If node >= 0, bind them to the given NUMA node
 */
void *callMmap(size_t sz, int node, int flags) {
	int prot = PROT_READ | PROT_WRITE;
	flags |= MAP_PRIVATE | MAP_ANONYMOUS;

	void *mem = mmap(0, sz, prot, flags, -1, 0);
	if (mem == MAP_FAILED) return 0;

	if (node >= 0) {
		bindMemory(mem, sz, node);
	}

	return mem;
}

template <class T>
class SyncVec : public std::vector<T> {
private:
	std::mutex _mutex;
public:
	SyncVec(const numa::Node &node) {
	}

	void addSynced(const T &v) {
		std::lock_guard<std::mutex> lock(_mutex);
		this->push_back(v);
	}
};

int main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Usage: %s size [writeRatio=1.0] [options]\n", argv[0]);
		printf("  size in megabytes\n");
		printf("  options:\n");
		printf("    quiet     only print result bandwidths\n");
		printf("    huge      allocate huge pages\n");
		printf("    wait      wait for keypress before exiting\n");
		return 1;
	}

	// read size
	size_t bytes;
	if (sscanf(argv[1], "%zu", &bytes) != 1) {
		printf("%s not a valid size", argv[1]);
		return 1;
	}
	bytes *= 1024 * 1024;

	// read write ratio
	float writeRatio = 1.0f;
	if (argc > 2 && sscanf(argv[2], "%g", &writeRatio) != 1) {
		printf("%s not a valid writeRatio", argv[2]);
		return 1;
	}

	// parse options
	bool quiet = false, huge = false, waitInput = false;
	for (int i = 3; i < argc; i++) {
		if (!strcmp(argv[i], "quiet")) quiet = true;
		if (!strcmp(argv[i], "huge")) huge = true;
		if (!strcmp(argv[i], "wait")) waitInput = true;
	}

	// params
	size_t pageSize = huge ? (1 << 21) : (1 << 12);
	size_t pageCount = (bytes + pageSize - 1) / pageSize;
	size_t pageWrites = (size_t)std::max(pageSize / sizeof(size_t) * writeRatio, 1.f);
	size_t writeMask = pageSize / sizeof(size_t) - 1;

	if (!quiet) {
		printf("pageSize: %zd\n", pageSize);
		printf("pageCount: %zd\n", pageCount);
		printf("pageWrites: %zd * %zd bytes\n", pageWrites, sizeof(size_t));
	}

	Timer<int> globalTimer(true);
	std::atomic_size_t threads(0);

	// startup worker threads
	numa::wait(numa::forEachThread(numa::NodeList::allNodes(), [&threads](){
		threads++;
	}, 0));
	int tStart = globalTimer.stop_get_start();

	numa::NodeReplicated<SyncVec<int>> threadTimers;

	// start paging
	numa::wait(numa::forEachThread(numa::NodeList::allNodes(), [=, &threadTimers]() {
		Timer<int> localTimer(true);

		int flags = huge ? (MAP_HUGETLB) : 0;
		void *data = callMmap(pageSize * pageCount, numa::Node::curr().physicalId(), flags);

		for (size_t p = 0; p < pageCount; p++) {
			size_t *page = (size_t*) ((char*)data + (p * pageSize));

			for (size_t i = 0; i < pageWrites; i++) {
				size_t idx = i & writeMask;
				page[idx] = i;
			}
		}

		threadTimers.get(numa::Node::curr()).addSynced(localTimer.stop_get());
	}, 0));
	int tFault = globalTimer.stop_get_start();

	size_t localMb = bytes / (1024*1024);
	size_t totalMb = localMb * threads.load();

	if (!quiet) {
	//	printf("tStart\t%d\n", tStart);
	//	printf("tFault\t%d\n", tFault);
	//	printf("totalSz\t%zdm\n", totalMb);
	//	printf("localSz\t%zdm\n", localMb);

	//	printf("totalBandwidth\t%8.2fm/s\n", totalMb * 1000 / (float)tFault);
	//	printf("localBandwidth\t%8.2fm/s\n", localMb * 1000 / (float)tFault);

		printf("time: %d\n", tFault + tStart);

		printf("node\ttotal\tlocal\tstd.dev\n");
	}

	for (numa::Node node : numa::NodeList::allNodes()) {
		const SyncVec<int> &times = threadTimers.get(node);

		// calc. local bandwidth for each thread
		std::vector<float> bw(times.size());
		std::transform(times.begin(), times.end(), bw.begin(), [localMb](int t) {
			return localMb * 1000 / (float)t;
		});

		// sum, avg
		float sum = std::accumulate(bw.begin(), bw.end(), 0.f, std::plus<float>());
		float avg = sum/times.size();

		// var, std.dev
		float var = std::accumulate(bw.begin(), bw.end(), 0.f, [avg](float a, float b) {
			return a + (avg-b) * (avg-b);
		});
		float stddev = sqrt(var);

		printf("%zd\t%d\t%d\t%d\n", node.physicalId(), (int)sum, (int)avg, (int)stddev);
	}

	if (waitInput)
		getc(stdin);

	return 0;
}
