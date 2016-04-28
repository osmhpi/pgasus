#pragma once

#include "base/node.hpp"
#include "msource/msource.hpp"

#include <vector>
#include <cassert>

namespace numa {

struct Place {
	MemSource       msource;
	Node            node;
	
	inline Place() : msource(), node() {}
	inline explicit Place(const Node &n) : msource(), node(n) {
		assert(node.valid());
	}
	inline explicit Place(const MemSource &ms) : msource(ms), node() {
		assert(ms.valid());
	}
	
	inline bool valid() const { 
		return msource.valid() || node.valid();
	}

	inline Node getNode() const {
		return (msource.valid()) ? msource.getLogicalNode() : node;
	}
};

namespace malloc {
typedef std::vector<Place> PlaceStack;
static constexpr size_t MEM_PAGE_SIZE = 4096;

void push(const Place &p);
void push_all(const PlaceStack &places);

Place pop();
PlaceStack pop_all();

MemSource curr_msource();
}

class PlaceGuard {
private:
	Place       _place;
public:
	inline PlaceGuard(const Node &node) : _place(node) { malloc::push(_place); }
	inline PlaceGuard(const MemSource &ms) : _place(ms) { malloc::push(_place); }
	inline PlaceGuard(const Place &p) : _place(p) { malloc::push(_place); }
	inline ~PlaceGuard() { malloc::pop(); }
};

namespace malloc {

class PlaceGuardForIfHack : public PlaceGuard {
public:
	inline PlaceGuardForIfHack(const Node &n) : PlaceGuard(n) {}
	inline PlaceGuardForIfHack(const MemSource &ms) : PlaceGuard(ms) {}
	inline PlaceGuardForIfHack(const Place &p) : PlaceGuard(p) {}
	inline operator bool() const { return true; }
};

}

#define numa_onplace(WHERE) if(PlaceGuardForIfHack __place_guard__(WHERE))

}

extern "C" {
	void free(void *p) __THROW;
	void* malloc(size_t sz) __THROW;
	void* calloc(size_t n, size_t sz) __THROW;
	void* realloc(void *p, size_t sz) __THROW;
	int posix_memalign(void** ptr, size_t align, size_t sz) __THROW;
	void* memalign(size_t align, size_t sz) __THROW;
	void* aligned_alloc(size_t alignment, size_t size) __THROW;
	void* valloc(size_t __size) __THROW;
	void* pvalloc(size_t __size) __THROW;
}

