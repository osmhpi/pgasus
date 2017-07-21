#include <cassert>
#include <errno.h>
#include <ext/alloc_traits.h>
#include <pthread.h>
#include <stdio.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "PGASUS/base/node.hpp"
#include "PGASUS/base/spinlock.hpp"
#include "PGASUS/malloc.hpp"
#include "PGASUS/msource/msource_types.hpp"
#include "base/topology.hpp"


namespace {

/**
 * Contains node-local msources
 */
class NodeLocalStorage
{
private:
	size_t                          node;
	size_t                          max_node_id;

	// Allocate one msource for each node, that is, an msource that allocates
	// memory from the given node. The data structures of the memsources will
	// stay on this node to reduce latency in case of cache misses
	numa::MemSource                 local_msource;
	numa::msvector<numa::MemSource> msources;

	numa::SpinLock                  lock;

public:
	NodeLocalStorage(const size_t n)
		: node(n)
		, max_node_id(numa::util::Topology::get()->node_ids().back())
		, local_msource(numa::MemSource::forNode(n))
		, msources(local_msource)
	{
		/**
		Note that "maxNodeID + 1 != nodeCount" on some systems.
		We allocate msources based on maxNodeID, so that the physical ID can
		always be used as index. Accepting that there can be gaps in the vector,
		wasting a few bytes of memory.
		*/

		assert(local_msource.valid());

		// make space for remote-node msources
		msources.resize(max_node_id + 1);
		msources[node] = local_msource;
	}

	const numa::MemSource& get(size_t n) {
		assert(n <= max_node_id);

		if (!msources[n].valid()) {
			std::lock_guard<numa::SpinLock> guard(lock);

			if (!msources[n].valid()) {
				char buff[4096];
				snprintf(buff, sizeof(buff) / sizeof(buff[0]), "nodeLocal(src=%zd dst=%zd)",
						node, n);
				msources[n] = numa::MemSource::create(n, 1LL<<24, buff, node);
			}
		}

		return msources[n];
	}

	~NodeLocalStorage() {
		msources.clear();
	}
};

numa::msvector<NodeLocalStorage*>& getNodeLocalStorages() {
	static auto nls = [] () {
		numa::msvector<NodeLocalStorage*> nls(numa::MemSource::global());
		const std::vector<int> &node_ids = numa::util::Topology::get()->node_ids();
		// There might be gaps in assigned NUMA node IDs, so that the node ID
		// cannot be used as index on a vector. Work around this by creating the
		// vector here with gaps if necessary.
		const size_t numReservedIds = node_ids.empty() ? 0u :
			static_cast<size_t>(node_ids.back() + 1u);	// node_ids is sorted
		nls.resize(numReservedIds, nullptr);
		for (const int i : numa::util::Topology::get()->node_ids())
			nls[static_cast<size_t>(i)] =
				numa::MemSource::forNode(size_t(i)).construct<NodeLocalStorage>(i);
		return nls;
	}();
	return nls;
}

/**
 * Contains thread-local msource
 */
struct ThreadLocalStorage
{
private:
	size_t                      _tid;
	size_t                      _node;
	NodeLocalStorage           *_node_storage;

	// may be NULL during initialization
	std::atomic_bool            _initializing;
	std::atomic_bool            _initialization_done;
	numa::MemSource             _thread_msource;

	// stack stored within local msource
	numa::msvector<numa::Place> _place_stack;

	// currently used mem source
	numa::MemSource             _curr_msource;

	static const size_t         s_local_source_size = (size_t)(1 << 24);

	inline bool is_inited() {
		return _initialization_done.load() || init();
	}

	inline const numa::MemSource& get_node_msource(int n) {
		return (n >= 0 && size_t(n) == _node)
			? _thread_msource
			: _node_storage->get(n);
	}

	inline const numa::MemSource& get_place_msource(const numa::Place &p) {
		return p.msource.valid() ? p.msource : get_node_msource(p.node.physicalId());
	}

	inline const numa::MemSource& get_curr_msource() {
		if (_place_stack.empty())
			return _thread_msource;

		return get_place_msource(_place_stack.back());
	}

public:

	ThreadLocalStorage()
		: _tid((size_t)-1)
		, _node()
		, _node_storage(nullptr)
		, _initializing(false)
		, _initialization_done(false)
		, _thread_msource()
		, _place_stack(numa::MemSource::global())
		, _curr_msource(numa::MemSource::global())
	{
	}

	/**
	 * Initialize object. Return true, if initialization complete
	 */
	bool init() {
		// only init once
		if (_initialization_done)
			return true;

		// no recursive init during static initializers
		if (_initializing)
			return false;

		_initializing = true;
		_tid = std::hash<std::thread::id>()(std::this_thread::get_id());

		// get node-local storage
		const numa::msvector<NodeLocalStorage*>& nodeLocals = getNodeLocalStorages();
		_node = numa::util::Topology::get()->curr_numa_node()->id;
		assert(_node < nodeLocals.size());
		_node_storage = nodeLocals[_node];
		assert(_node_storage);

		// create msource
		char buff[4096];
		snprintf(buff, sizeof(buff) / sizeof(buff[0]), "local(%zX)", _tid % 0xFFFFFFFF);
		_thread_msource = numa::MemSource::create(_node, s_local_source_size, buff);

		// create stack from local msource
		_place_stack = numa::msvector<numa::Place>(_thread_msource);
		_curr_msource = _thread_msource;
		_initialization_done = true;

		assert(_thread_msource.valid());

		return true;
	}

	~ThreadLocalStorage() {
	}

	// Push a Place onto the allocation-source stack
	inline void push(const numa::Place &p) {
		_place_stack.push_back(p);
		_curr_msource = get_place_msource(p);
	}

	// Pop the top MemSource from the allocation-source stack
	inline numa::Place pop() {
		assert(!_place_stack.empty());

		numa::Place result = _place_stack.back();
		_place_stack.pop_back();
		_curr_msource = get_curr_msource();
		return result;
	}

	// Moves the whole current stack into given collection
	template <class Col>
	inline void pop_all(Col &dst) {
		dst.insert(dst.end(), _place_stack.begin(), _place_stack.end());
		_place_stack.clear();
		_curr_msource = _thread_msource;
	}

	// Copies the whole given collection onto msource stack
	template <class Col>
	inline void push_all(const Col &dst) {
		_place_stack.insert(_place_stack.end(), dst.begin(), dst.end());
		_curr_msource = get_curr_msource();
	}

	// Get the current allocation MemSource (may be top of stack or thread-local)
	// During initialization, this might return the global allocator
	inline const numa::MemSource& get_msource() {
		return _curr_msource;
	}
};

//
// TLS deleter
//
void tlsDestructionFunc(void *data) {
	ThreadLocalStorage *ptr = (ThreadLocalStorage*) data;
	if (ptr != nullptr) {
		numa::MemSource::global().destruct(ptr);
	}
}

//
// PThread keys
//
pthread_key_t createKey(void (*destr)(void*)) {
	pthread_key_t key;
	pthread_key_create(&key, destr);
	return key;
}

pthread_key_t& getKey() {
	static pthread_key_t key = createKey(tlsDestructionFunc);
	return key;
}

// make sure key is created upon program startup
struct GetKeyInitializer {
	GetKeyInitializer() { getKey(); }
} s_getKey;

//
// TLS getter
//
ThreadLocalStorage &getTlsImpl() {
	// the order is important here, as ThreadLocalStorage::init() will probably call malloc() again.
	void *ptr = pthread_getspecific(getKey());
	if (ptr == nullptr) {
		ptr = (void*) numa::MemSource::global().construct<ThreadLocalStorage>();
		pthread_setspecific(getKey(), ptr);
		((ThreadLocalStorage*) ptr)->init();
	}

	return *((ThreadLocalStorage*) ptr);
}

ThreadLocalStorage &getTls() {
	ThreadLocalStorage &tls = getTlsImpl();
	return tls;
}

}



namespace numa {
namespace malloc {

void push(const Place &p) {
	assert(p.valid());
	getTls().push(p);
}

void push_all(const PlaceStack &places) {
	assert(std::all_of(places.begin(), places.end(),
		std::bind(&Place::valid, std::placeholders::_1)));
	getTls().push_all(places);
}

Place pop() {
	return getTls().pop();
}

PlaceStack pop_all() {
	PlaceStack result;
	getTls().pop_all(result);
	return result;
}

MemSource curr_msource() {
	return getTls().get_msource();
}

}
}


#ifndef NUMA_STACKEDMALLOC_DEBUG
#define NUMA_STACKEDMALLOC_DEBUG 0
#endif

/*
 * Global C and POSIX memory allocation functions
 */

inline void *stackedmalloc(size_t sz) throw() {
	void *result = getTls().get_msource().alloc(sz);

#if NUMA_STACKEDMALLOC_DEBUG
	printf("[alloc] sz=%zd source=%s result=%p\n", sz, getTls().get_msource().getDescription().c_str(), result);
	fflush(stdout);
#endif

	return result;
}

inline void *stackedmalloc_aligned(size_t align, size_t sz) throw() {
	if (sz == 0)
		return nullptr;

	void *result = getTls().get_msource().allocAligned(align, sz);

#if NUMA_STACKEDMALLOC_DEBUG
	printf("[align] sz=%zd align=%zd source=%s result=%p\n", sz, align, getTls().get_msource().getDescription().c_str(), result);
	fflush(stdout);
#endif

	return result;
}


extern "C" void free(void *p) throw() {
#if NUMA_STACKEDMALLOC_DEBUG
	if (p != nullptr) { printf("[free] %p\n", p); fflush(stdout); }
#endif
	numa::MemSource::free(p);
}

extern "C" void* malloc(size_t sz) throw() {
	return stackedmalloc(sz);
}

extern "C" void* calloc(size_t n, size_t sz) throw() {
	size_t total = n * sz;
	void *ptr = stackedmalloc(total);
	if (ptr != nullptr)
		memset(ptr, 0, total);
	return ptr;
}

extern "C" void* realloc(void *p, size_t sz) throw() {
	// don't bother if the block is to be shrinked
	size_t old_size = (p != nullptr) ? numa::MemSource::allocatedSize(p) : 0;
	if (sz < old_size)
		return p;

	// allocate new
	void *pnew = stackedmalloc(sz);
	if (pnew == nullptr)
		return nullptr;

	if (p != nullptr) {
		memmove(pnew, p, old_size);
		numa::MemSource::free(p);
	}

	return pnew;
}

extern "C" int posix_memalign(void** ptr, size_t align, size_t sz) throw() {
	if (sz == 0) {
		*ptr = nullptr;
		return 0;
	}

	void *p = stackedmalloc_aligned(align, sz);
	if (p == nullptr) {
		return ENOMEM;
	}

	*ptr = p;
	return 0;
}

extern "C" void* memalign(size_t align, size_t sz) throw() {
	return stackedmalloc_aligned(align, sz);
}

extern "C" void* aligned_alloc(size_t alignment, size_t size) throw() {
	// The function aligned_alloc() is the same as memalign(), except for the
	// added restriction that size should be a multiple of alignment.
	if (size % alignment != 0) {
		errno = EINVAL;
		return nullptr;
	}
	return stackedmalloc_aligned(alignment, size);
}

extern "C" void* valloc(size_t __size) throw() {
	return stackedmalloc_aligned(numa::malloc::MEM_PAGE_SIZE, __size);
}

extern "C" void* pvalloc(size_t __size) throw() {
	__size = (__size + numa::malloc::MEM_PAGE_SIZE-1) % numa::malloc::MEM_PAGE_SIZE;
	return stackedmalloc_aligned(numa::malloc::MEM_PAGE_SIZE, __size);
}

extern "C" void malloc_stats(void) throw() {
}

extern "C" int mallopt(int cmd, int value) throw() {
  return 0;
}

