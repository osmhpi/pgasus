#include "nodehelper.h"

#include <cassert>
#include <cstdint>
#include <utility>

#include <pthread.h>
#include <sched.h>

#include "base/node.hpp"
#include "msource/mmaphelper.h"

static void* thread_func(void *ptr) {
	std::function<void()> *function = (std::function<void()>*) ptr;
	(*function)();
	return nullptr;
}

void runAt(numa::Node node, const std::function<void()> &function) {
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);

	// set cpuset from node
	for (int core : node.cpuids())
		CPU_SET(core, &cpu_set);

	pthread_attr_t attr;
	pthread_t handle;
	assert(pthread_attr_init(&attr) == 0);
	assert(pthread_attr_setaffinity_np(&attr, sizeof(cpu_set), &cpu_set) == 0);
	assert(pthread_create(&handle, &attr, thread_func, (void*) &function) == 0);
	assert(pthread_join(handle, nullptr) == 0);
}

int NodeLocalityChecker::get(void *p) {
	static constexpr uintptr_t PAGE_SIZE = 4096;
	static constexpr uintptr_t PAGE_MASK = ~(PAGE_SIZE-1);

	uintptr_t page = reinterpret_cast<uintptr_t>(p);
	page &= PAGE_MASK;

	auto it = pageNodes.find(page);

	if (it == pageNodes.end()) {
		int n = getNumaNodeForMemory((void*) page);
		pageNodes[page] = n;
		return n;
	} else {
		return it->second;
	}
}

void NodeLocalityChecker::clear() {
	pageNodes.clear();
}
