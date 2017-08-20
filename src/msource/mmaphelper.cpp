#include "PGASUS/msource/mmaphelper.h"

#include <numaif.h>      /* for mbind */
#include <numa.h>

#include <cstdint>
#include <cassert>
#include <string.h>


/** 
 * Allocate sz bytes from system. If node >= 0, bind them to the given NUMA node
 */
extern "C" void *callMmap(size_t sz, int node) {
	int prot = PROT_READ | PROT_WRITE;
	int flags = MAP_PRIVATE | MAP_ANONYMOUS;
	
	void *mem = mmap(0, sz, prot, flags, -1, 0);
	if (mem == MAP_FAILED) return 0;
	
	if (node >= 0) {
		bindMemory(mem, sz, node);
	}
	
	return mem;
}

/**
 * Bind memory region to given NUMA node
 */
extern "C" int bindMemory(void *p, size_t sz, int node) {
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
 * Get page for given pointer
 */
inline static void* align_page(const void *ptr) {
	static constexpr uintptr_t PAGE_MASK = ~(uintptr_t(0xFFF));
	return (void*) (((uintptr_t)ptr) & PAGE_MASK);
}

/**
 * Return NUMA node location of data pointed to by ptr, as returned by move_pages
 */
extern "C" int getNumaNodeForMemory(const void *ptr) {
	int result, loc;
	void *pptr = align_page(ptr);
	
	result = numa_move_pages(0, 1, &pptr, nullptr, &loc, 0);
	
	return (result != 0) ? -1 : loc;
}

/**
 * Stores NUMA node locations of data pointed to by ptr into results.
 */
extern "C" int getNumaNodeForMemory_n(size_t count, const void **ptrs, int *results) {
	std::vector<void*> pages(count);
	
	for (size_t i = 0; i < count; i++) {
		pages[i] = align_page(ptrs[i]);
	}
	
	return numa_move_pages(0, count, &pages[0], nullptr, results, 0);
}

/**
 * Move memory region to given node
 */
extern "C" int moveMemory(void *p, size_t sz, int node) {
	std::vector<void*> pages(sz / 4096 + 1);

	char *begin = (char*) p;
	char *end = begin + sz;

	for (char *piter = (char*) align_page(p); piter < end; piter += 4096) {
		pages.push_back((void*) piter);
	}

	std::vector<int> dst(pages.size(), node);
	std::vector<int> status(pages.size(), 0);

	return numa_move_pages(0, pages.size(), pages.data(), dst.data(), status.data(), MPOL_MF_MOVE);
}

/**
 * Touches every page from given memory region, to make it page-fault into working set
 */
void touchMemory(void *p, size_t sz) {
	for (size_t ofs = 0; ofs < sz; ofs += 4096) {
		size_t *ptr = (size_t*) ((char*)p + ofs);
		*((size_t volatile *) ptr) = *ptr;
	}
}

namespace numa {
namespace util {

/**
 * Get NUMA node locations for given chunk of memory
 */
std::vector<int> getNumaNodeForMemory(const void *ptr, size_t sz) {
	const char *base = (const char*) align_page(ptr);
	const char *end = (const char*) align_page((void*) ((char*)ptr + sz));

	std::vector<const void*> pages;
	pages.reserve(sz / 4096 + 1);
	for (const char *p = base; p <= end; p += 4096)
		pages.push_back((const void*) p);

	std::vector<int> ret(pages.size());
	if (getNumaNodeForMemory_n(pages.size(), &pages[0], &ret[0]) != 0) {
		for (auto &i : ret) i = -1;
	}
	return ret;
}

}
}
