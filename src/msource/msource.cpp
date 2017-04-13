#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstddef>
#include <cassert>
#include <cstring>
#include <mutex>
#include <vector>

#include <numaif.h>

#include "malloc-numa.h"
#include "msource_allocator.hpp"
#include "base/spinlock.hpp"
#include "msource/msource_types.hpp"
#include "msource/mmaphelper.h"
#include "util/topology.hpp"
#include "util/debug.hpp"


namespace numa {
namespace msource {

class MemSourceImpl
{
private:

#ifdef MEM_SOURCE_USE_PTHREAD_SPINLOCK
	typedef pthread_spinlock_t SpinLock;
	#define SpinLock_lock(s) pthread_spin_lock(&s)
	#define SpinLock_unlock(s) pthread_spin_unlock(&s)
	#define SpinLock_init(s) (pthread_spin_init(&s, PTHREAD_PROCESS_PRIVATE) == 0)
	#define SpinLock_destroy(s) (pthread_spin_destroy(&s) == 0)
#else
	typedef numa::SpinLockType<LinearBackOff<256,4096>> SpinLock;
	#define SpinLock_lock(s) s.lock()
	#define SpinLock_unlock(s) s.unlock()
	#define SpinLock_init(s) (true)
	#define SpinLock_destroy(s) (true)
#endif

#ifndef MEM_SOURCE_FILL_MEMORY_DEBUG
#define MEM_SOURCE_FILL_MEMORY_DEBUG 0
#endif

	// alignment mask
	static constexpr inline intptr_t ALIGN_MASK(intptr_t s) {
		return s - (intptr_t) 1;
	}

	// number of bytes to offset an address to align it
	static constexpr inline intptr_t ALIGN_UP(intptr_t p, intptr_t s) {
		return (p + ALIGN_MASK(s)) & ~ALIGN_MASK(s);
	}
	static constexpr inline intptr_t ALIGN_DOWN(intptr_t p, intptr_t s) {
		return p & ~ALIGN_MASK(s);
	}
	static constexpr inline bool IS_ALIGNED(intptr_t p, intptr_t s) {
		return (p & ALIGN_MASK(s)) == 0;
	}

	// forward decl
	struct Arena;

	template <class T, class S>
	static constexpr inline T *cast(S *s, ssize_t ofs = 0) {
		return reinterpret_cast<T*>(reinterpret_cast<char*>(s) + ofs);
	}

	// footer that is placed under each allocated block
	struct ChunkFooter
	{
		MemSourceImpl  *source;

		union {
			Arena       *arena; // NULL if allocated with mmap
			ChunkFooter *link;  // if source==NULL, this points to actual footer
		};

		static constexpr inline intptr_t DATA_OFFSET() {
			return ALIGN_UP(sizeof(ChunkFooter), 2*sizeof(intptr_t));
		}

		static inline ChunkFooter* FROM_POINTER(void *p) {
			return cast<ChunkFooter>(p, -DATA_OFFSET());
		}

		inline void* TO_POINTER() {
			return cast<void>(this, DATA_OFFSET());
		}
	};

	// footer that is placed under each mmap-allocated block
	// (includes regular footer)
	struct MmapChunkFooter
	{
		size_t size;

		// Double-linked list of all mmapped-chunks within one msource
		MmapChunkFooter *prev;
		MmapChunkFooter *next;

		// this must be last. must be at the exact end of this struct.
		ChunkFooter footer;

		static constexpr inline intptr_t DATA_OFFSET() {
			static_assert(
				offsetof(MmapChunkFooter, footer)
					+ sizeof(ChunkFooter)
					== sizeof(MmapChunkFooter), "Footer alignment wrong");
			return sizeof(MmapChunkFooter);
		}

		static inline MmapChunkFooter* FROM_POINTER(void *p) {
			return cast<MmapChunkFooter>(p, -DATA_OFFSET());
		}

		inline void* TO_POINTER() {
			return cast<void>(this, DATA_OFFSET());
		}
	};

	// memory region that is managed by dlmalloc's mspace
	struct Arena
	{
		MemSourceImpl              *msource;

		size_t                  size;      // total space in mspace
		size_t                  alloc_end; // no allocated bytes behind this offset
		bool                    native;    // mspace is directly behind arena data?

		void                   *base;      // start of msp
		mspace                  msp;

		SpinLock                mspace_lock;

		// Double-linked list of all arenas within one msource
		Arena                  *prev;
		Arena                  *next;

		// if dst_node >= 0: directly allocate on node. If < 0, use adjacent mem
		Arena(MemSourceImpl *ms, size_t sz, int dst_node = -1) {
			msource = ms;
			alloc_end = MEM_PAGE_SIZE;
			prev = nullptr;
			next = nullptr;
			if (!SpinLock_init(mspace_lock)) {
                assert(false);
            }

			// create mspace
			if (dst_node < 0) {
				base = (void*) ALIGN_UP((intptr_t)this + sizeof(*this), 64);
				size = ((intptr_t)this + sz) - (intptr_t)base;
				native = true;
			} else {
				base = callMmap(sz, dst_node);
				size = sz;
				native = false;
			}

			msp = create_mspace_with_base(base, size, 0);

			prev = nullptr;
			next = nullptr;
		}

		~Arena() {
			destroy_mspace(msp);

			if (!native) {
				munmap(base, size);
			}

			if (!SpinLock_destroy(mspace_lock)) {
                assert(false);
            }
		}

		inline ChunkFooter *alloc(size_t sz) {
			SpinLock_lock(mspace_lock);

			size_t alloc_size = sz + ChunkFooter::DATA_OFFSET();

			ChunkFooter *chunk = static_cast<ChunkFooter*>(mspace_malloc(msp, alloc_size));

			if (chunk != nullptr) {
				// update alloc end ptr
				size_t rel_chunk_start = (intptr_t)chunk - (intptr_t)this;
				alloc_end = std::max(alloc_end, rel_chunk_start + sz);

				chunk->source = msource;
				chunk->arena = this;
			}

			SpinLock_unlock(mspace_lock);

			return chunk;
		}

		inline void free(void *p, ChunkFooter *ch) {
			SpinLock_lock(mspace_lock);
		#if MEM_SOURCE_FILL_MEMORY_DEBUG
			size_t clearSize = (uintptr_t)p - (uintptr_t)ch;
			memset((void*) ch, 0xCC, clearSize);
		#endif
			mspace_free(msp, (void*) ch);
			SpinLock_unlock(mspace_lock);
		}

		size_t prefault(size_t bytes) {
			size_t max = std::min(bytes, size);
			touchMemory((void*) base, max);
			return max;
		}
	};

	// stores in one atomic word:
	//  - number of allocated blocks
	//  - number of references
	// if this counter reaches zero, the source can be destroyed.
	class BlockCount {
	private:
		std::atomic_size_t _value;
		static constexpr size_t BlockBits = 40;
		static constexpr size_t RefBits = (8*sizeof(size_t)) - BlockBits;
		static constexpr size_t BlockMask = ((size_t)1 << BlockBits) - 1;

		inline bool isLastDeref(size_t delta) {
			return _value.fetch_sub(delta) == delta;
		}

	public:
		BlockCount() : _value(0) {}
		inline size_t blocks() const {
			return _value.load() & BlockMask;
		}
		inline size_t refs() const {
			return _value.load() >> BlockBits;
		}
		inline void ref() {
			_value.fetch_add((size_t)1 << BlockBits);
		}
		inline bool deref() {
			return isLastDeref((size_t)1 << BlockBits);
		}
		inline void addBlock() {
			_value.fetch_add(1);
		}
		inline bool removeBlock() {
			return isLastDeref(1);
		}

	};

	static constexpr size_t     NAME_LENGTH = 128;
	char                        description[NAME_LENGTH];

	void                       *user_data;

	int                         node;             // where the memory comes from
	int                         node_home;        // where the data structs lie, or -1, if its the same

	size_t                      mmap_threshold;
	size_t                      mem_size;

	SpinLock                    arena_lock;
	Arena                      *native_arena;
	Arena                      *active_arena;

	// List of all mmapped-chunks
	SpinLock                    mmapped_chunk_lock;
	MmapChunkFooter            *mmapped_chunk_head;

	BlockCount                  blocks;

	// performance counters
	//std::atomic_size_t          allocs_total;
	//std::atomic_size_t          alloc_size_curr;
	//std::atomic_size_t          alloc_size_total;
	//std::atomic_size_t          alloc_size_max;

private:

	MemSourceImpl(int n, size_t sz, const char *str, int home) {
		strncpy(description, str, NAME_LENGTH);

		// Init msource
		node = n;
		node_home = home;
		mmap_threshold = (size_t)(1 << 18);
		mem_size = sz;
		mmapped_chunk_head = nullptr;

		// Create native arena directly behind source header
		void *arena_start = (void*) ALIGN_UP((intptr_t)this + sizeof(*this), 64);
		size_t arena_size = ((intptr_t)this + sz) - (intptr_t)arena_start;
		int dst_node = (node_home >= 0) ? node : -1;
		native_arena = new (arena_start) Arena(this, arena_size, dst_node);
		active_arena = native_arena;

		// Init spinlocks
		if (!SpinLock_init(arena_lock)) {
            assert(false);
        }
		if (!SpinLock_init(mmapped_chunk_lock)) {
            assert(false);
        }
	}

	~MemSourceImpl() {
		assert(blocks.refs() == 0 && blocks.blocks() == 0);

		// destroy all mspace arenas and return their mem to the system
		Arena *arena_curr = active_arena;
		Arena *arena_next = nullptr;

		while (arena_curr) {
			arena_next = arena_curr->next;
			size_t sz = arena_curr->size;

			// delete arena
			arena_curr->~Arena();

			// Only return non-native arena memory
			if (arena_curr != native_arena) {
				munmap(arena_curr, sz);
			}

			arena_curr = arena_next;
		}

		// return directly mmapped memory blocks
		MmapChunkFooter *mch_curr = mmapped_chunk_head;
		MmapChunkFooter *mch_next = nullptr;
		while (mch_curr != nullptr) {
			mch_next = mch_curr->next;
			munmap(mch_curr, mch_curr->size);
			mch_curr = mch_next;
		}

		// Destroy spinlocks
		if (!SpinLock_destroy(arena_lock)) {
            assert(false);
        }
		if (!SpinLock_destroy(mmapped_chunk_lock)) {
            assert(false);
        }
	}

	inline Arena* create_new_arena(size_t arena_size)
	{
		assert (active_arena);
		assert (!active_arena->prev);

		// Allocate system memory
		int where = (node_home >= 0) ? node_home : node;
		void *mem = callMmap(arena_size, where);
		if (mem == nullptr) return nullptr;

		int dst_node = (node_home >= 0) ? node : -1;
		Arena *arena = new (mem) Arena(this, arena_size, dst_node);
		arena->next = active_arena;
		active_arena->prev = arena;
		active_arena = arena;

		return arena;
	}

	bool free_impl(void *p, ChunkFooter *ch) {
		if (ch->arena != nullptr) {
			ch->arena->free(p, ch);
			// TODO update alloc end ptr?
		}
		else {
			MmapChunkFooter *mch = MmapChunkFooter::FROM_POINTER(p);

			SpinLock_lock(mmapped_chunk_lock);

			// remove from mmap-chunk list
			if (!mch->prev) {
				mmapped_chunk_head = mch->next;
			} else {
				mch->prev->next = mch->next;
			}

			if (mch->next) {
				mch->next->prev = mch->prev;
			}

			SpinLock_unlock(mmapped_chunk_lock);

			munmap(mch, mch->size);
		}

		bool ret = blocks.removeBlock();

		return ret;
	}

	static inline ChunkFooter* get_footer_for_mem(void *p) {
		ChunkFooter *ch = ChunkFooter::FROM_POINTER(p);

		// source=null -> the actual chunk starts earlier
		while (ch->source == nullptr) {
			ch = ch->link;
		}

		return ch;
	}

	static void add_pages_to_vector(std::vector<void*> &v, void *start, void *end) {
		intptr_t istart = ALIGN_DOWN((intptr_t) start, MEM_PAGE_SIZE);
		intptr_t iend = ALIGN_DOWN((intptr_t) end, MEM_PAGE_SIZE);
		for (intptr_t i = istart; i <= iend; i += MEM_PAGE_SIZE) {
			v.push_back((void*) i);
		}
	}

	typedef std::vector<MemSourceImpl*, numa::util::MmapAllocator<MemSourceImpl*>> MemSourceVector;

	static numa::SpinLock& s_allsources_lock() {
		static numa::SpinLock lock;
		return lock;
	}

	static void add_msource(MemSourceImpl *ms) {
		std::lock_guard<numa::SpinLock> g(s_allsources_lock());
		s_allsources().push_back(ms);
	}

	static void remove_msource(MemSourceImpl *ms) {
		std::lock_guard<numa::SpinLock> g(s_allsources_lock());
		MemSourceVector &msv = s_allsources();
		for (size_t i = 0; i < msv.size(); i++)
			if (msv[i] == ms)
				msv[i] = nullptr;
	}

public:

	static MemSourceVector& s_allsources() {
		static MemSourceVector *msv = new (callMmap(4096, -1)) MemSourceVector();
		return *msv;
	}

	static MemSourceImpl *create(int phys_node, size_t sz, const char *str, int phys_home_node) {
		MemSourceImpl *ms = new (callMmap(sz, phys_node)) MemSourceImpl(phys_node, sz, str, phys_home_node);
		add_msource(ms);
		return ms;
	}

	static void destroy(MemSourceImpl *ms) {
		remove_msource(ms);
		size_t sz = ms->mem_size;
		ms->~MemSourceImpl();
		munmap(ms, sz);
	}

	void ref() {
		blocks.ref();
	}

	void unref() {
		// last reference deleted -> print info
		if (blocks.refs() == 1) {
			char buff[4096];
			getDescription(buff, 4096);
			numa::debug::log(numa::debug::DEBUG, "Abandon MemSource %s", buff);
		}

		// last usage -> delete
		if (blocks.deref()) {
			destroy(this);
		}
	}

	void getDescription(char *buffer, size_t sz) {
		snprintf(buffer, sz, "%s [%p] n=%d blks=%zd",
			description,
			(void*) this,
			node,
			blocks.blocks());
	}

	static inline size_t get_block_size(void *p) {
		ChunkFooter *ch = get_footer_for_mem(p);

		if (ch->arena != nullptr) {
			return dlmalloc_usable_size((void*)ch) - ChunkFooter::DATA_OFFSET();
		} else {
			return MmapChunkFooter::FROM_POINTER(p)->size;
		}
	}

	inline int get_node() const {
		return node;
	}

	size_t migrate(int dst) {
		std::vector<void*> pages;

		// acquire all locks
		SpinLock_lock(arena_lock);
		SpinLock_lock(mmapped_chunk_lock);

		// iterate through all arenas, add arena spaces
		// (this includes the main arena and the MemSource)
		for (Arena *curr = active_arena; curr != nullptr; curr = curr->next) {
			SpinLock_lock(curr->mspace_lock);

			void *top_chunk;
			size_t top_chunk_size;

			mspace_get_top_chunk_extent(curr->msp, &top_chunk, &top_chunk_size);
			add_pages_to_vector(pages, (void*)curr->msp, top_chunk);

			// page-out unused space - will be paged in at new home node on demand
			intptr_t dn_start = ALIGN_UP((intptr_t) top_chunk + 64, MEM_PAGE_SIZE);
			intptr_t dn_end = ALIGN_DOWN((intptr_t) top_chunk + top_chunk_size - 64, MEM_PAGE_SIZE);
			madvise((void*) dn_start, dn_end - dn_start, MADV_DONTNEED);
		}

		// add directly mmapped memory blocks
		for (MmapChunkFooter *curr = mmapped_chunk_head; curr != nullptr; curr = curr->next) {
			char *end = (char*)curr + curr->size;
			add_pages_to_vector(pages, (void*)curr, (void*)end);
		}

		// call move_pages
		std::vector<int> dst_vec(pages.size(), dst);
		std::vector<int> status_vec(pages.size(), 0);
		if (move_pages(0, pages.size(), &pages[0], &dst_vec[0], &status_vec[0], MPOL_MF_MOVE) != 0) {
			perror("MemSource::migrate(): move_pages()");
			fprintf(stderr, "MemSource::migrate(): pages/status = [\n\t");
			for (size_t i = 0; i < pages.size(); i++) {
				fprintf(stderr, "%p (%d)", pages[i], status_vec[i]);
				if (i == pages.size()-1) fprintf(stderr, "\n]\n");
				else if (i % 6 == 5) fprintf(stderr, "\n\t");
			}
		}

		// release all locks
		SpinLock_unlock(arena_lock);
		SpinLock_unlock(mmapped_chunk_lock);
		for (Arena *curr = active_arena; curr != nullptr; curr = curr->next)
			SpinLock_unlock(curr->mspace_lock);

		node = dst;

		return pages.size();
	}

	inline void *alloc(size_t bytes)
	{
		void *result = nullptr;

		// Directly allocate system memory?
		if (bytes >= mmap_threshold) {
			size_t sz = bytes + MmapChunkFooter::DATA_OFFSET();
			void *mem = callMmap(sz, node);
			if (mem == nullptr) return nullptr;

			// Init chunk header
			MmapChunkFooter *chunk = static_cast<MmapChunkFooter*>(mem);
			chunk->footer.source = this;
			chunk->footer.arena = nullptr;
			chunk->size = sz;

			// Insert into msource's mmap chunk list
			chunk->prev = nullptr;
			SpinLock_lock(mmapped_chunk_lock);
			chunk->next = mmapped_chunk_head;
			if (mmapped_chunk_head != nullptr) {
				mmapped_chunk_head->prev = chunk;
			}
			mmapped_chunk_head = chunk;
			SpinLock_unlock(mmapped_chunk_lock);

			// Finalize
			result = chunk->TO_POINTER();

			goto done;
		}
		else {
			SpinLock_lock(arena_lock);

			// see if the active arena that can satisfy the request
			ChunkFooter *arena_chunk = active_arena->alloc(bytes);

			// if not, see if another arena may qualify as the new active arena (TODO)

			// if not, create new arena as active arena
			if (arena_chunk == nullptr) {
				Arena *arena = create_new_arena((size_t)(64 << 20));
				if (arena != nullptr) {
					arena_chunk = arena->alloc(bytes);
				}
			}

			SpinLock_unlock(arena_lock);

			if (arena_chunk == nullptr)
				return nullptr;

			result = arena_chunk->TO_POINTER();

			goto done;
		}

	done:
		blocks.addBlock();

		return result;
	}

	inline void *alloc_align(size_t align, size_t sz) {
		// alloc larger size - to make room for alignment and fake footer
		size_t alloc_size = sz + align + ChunkFooter::DATA_OFFSET();
		void *p = alloc(alloc_size);

		intptr_t pint = (intptr_t) p;

		// already aligned?
		if (IS_ALIGNED(pint, align))
			return p;

		intptr_t pint_new = ALIGN_UP(pint + ChunkFooter::DATA_OFFSET(), align);

		// store address of actual chunk footer into "fake" footer
		ChunkFooter *chunk = ChunkFooter::FROM_POINTER(p);
		ChunkFooter *fake_chunk = ChunkFooter::FROM_POINTER((void*)pint_new);
		fake_chunk->source = nullptr;
		fake_chunk->link = chunk;

		return fake_chunk->TO_POINTER();
	}

	static inline void free(void *p)
	{
		if (p != nullptr) {
			ChunkFooter *ch = get_footer_for_mem(p);
			MemSourceImpl *src = ch->source;

			// if this was the last chunk allocated, and the msource is already
			// abandoned, destroy the msource now
			if (src->free_impl(p, ch)) {
				destroy(src);
			}
		}
	}

	static inline int physicalNodeOf(void *p) {
		if (p == nullptr) return -1;
		ChunkFooter *ch = get_footer_for_mem(p);
		return ch->source->get_node();
	}

	struct msource_info stats()
	{
		struct msource_info result;

		result.arena_count = 0;
		result.arena_used = 0;
		result.arena_size = 0;
		result.hugeobj_count = 0;
		result.hugeobj_used = 0;
		result.hugeobj_size = 0;

		Arena *arena;
		MmapChunkFooter *mch;

		// count every mspace arena - this includes the actual msource struct
		// (native arena)
		for (arena = active_arena; arena; arena = arena->next) {
			intptr_t base = ALIGN_DOWN((intptr_t)arena, MEM_PAGE_SIZE);
			intptr_t end = (intptr_t)arena + arena->alloc_end;

			struct mallinfo minfo = mspace_mallinfo(arena->msp);

			result.arena_used += ALIGN_UP(end-base, MEM_PAGE_SIZE);
			result.arena_size += minfo.uordblks;
			result.arena_count += 1;
		}

		// count mmapped chunks
		for (mch = mmapped_chunk_head; mch; mch = mch->next) {
			result.hugeobj_used += ALIGN_UP(mch->size, MEM_PAGE_SIZE);
			result.hugeobj_size += mch->size;
			result.hugeobj_count += 1;
		}

		return result;
	}

	size_t prefault(size_t bytes) {
		return native_arena->prefault(bytes);
	}
};

// debug output msources before program shutdown - this should
// be handled in stackedmalloc, etc, where msources are allocated.
// but TLS support is a grand fuckup.
static struct StaticMsourceDtor {
	StaticMsourceDtor() {
	}
	~StaticMsourceDtor() {
		for (MemSourceImpl *ms : MemSourceImpl::s_allsources()) {
			if (ms == nullptr) continue;
			char buff[4096];
			ms->getDescription(buff, 4096);
			numa::debug::log(numa::debug::DEBUG, "OnExit MemSource %s", buff);
		}
	}
} _static_msource_dtor;


} // namespace msource

MemSource::MemSource() : _msource(nullptr) {}

MemSource::MemSource(msource::MemSourceImpl *ms) : _msource(ms) {
	assert(ms != nullptr);
	ms->ref();
}

MemSource::MemSource(const MemSource &other)
	: _msource(other._msource)
{
	if (_msource != nullptr) _msource->ref();
}

MemSource::MemSource(MemSource &&other)
	: _msource(other._msource)
{
	other._msource = nullptr;
}

MemSource::~MemSource() {
	if (_msource != nullptr) _msource->unref();
	_msource = nullptr;
}

MemSource& MemSource::operator=(const MemSource &other) {
	if (other._msource != _msource) {
		msource::MemSourceImpl *old = _msource;
		_msource = other._msource;
		if (_msource != nullptr) _msource->ref();
		if (old != nullptr) old->unref();
	}
	return *this;
}

MemSource& MemSource::operator=(MemSource &&other) {
	if (_msource != nullptr)
		_msource->unref();

	_msource = other._msource;
	other._msource = nullptr;

	return *this;
}

MemSource MemSource::create(int phys_node, size_t sz, const char *str, int phys_home_node) {
	msource::MemSourceImpl *impl = msource::MemSourceImpl::create(phys_node, sz, str, phys_home_node);
	numa::debug::log(numa::debug::DEBUG, "Created MemSource \"%s\" on node %d", str, phys_node);
	return MemSource(impl);
}

const MemSource& MemSource::global() {
	static MemSource                global_msource;
	static numa::SpinLock           global_msource_mutex;
	bool valid;

	{
		std::lock_guard<numa::SpinLock> lock(global_msource_mutex);
		if (!(valid = global_msource.valid()))
			global_msource = MemSource(msource::MemSourceImpl::create(-1, 1LL<<24, "global", -1));
	}

	if (!valid)
		numa::debug::log(numa::debug::DEBUG, "Created global MemSource");

	return global_msource;
}

const MemSource& MemSource::forNode(size_t phys_node) {
	static msource::MemSourceImpl **global_msources = nullptr;
	static msvector<MemSource>      global_msource_ptrs(global());
	static size_t                   global_msources_count = 0;
	static numa::SpinLock           global_msources_mutex;

	std::lock_guard<numa::SpinLock> lock(global_msources_mutex);

	if (global_msources == nullptr) {
		global_msources_count = numa::util::Topology::get()->max_node_id() + 1;
		size_t sz = sizeof(msource::MemSourceImpl*) * global_msources_count;

		global_msources = (msource::MemSourceImpl**) global().allocAligned(64, sz);
		for (size_t i = 0; i < global_msources_count; i++)
			global_msources[i] = nullptr;

		global_msource_ptrs.resize(global_msources_count);
	}

	assert(phys_node >= 0 && phys_node < global_msources_count);

	if (global_msources[phys_node] == nullptr) {
		char buff[4096];
		snprintf(buff, sizeof(buff) / sizeof(buff[0]), "node_global(%zd)", phys_node);
		global_msources[phys_node] = msource::MemSourceImpl::create(phys_node, 1LL<<24, buff, -1);
		global_msource_ptrs[phys_node] = MemSource(global_msources[phys_node]);

		numa::debug::log(numa::debug::DEBUG, "Created nodeGlobal MemSource (%zd)", phys_node);
	}

	return global_msource_ptrs[phys_node];
}

void* MemSource::alloc(size_t sz) const {
	void *ret = _msource->alloc(sz);
#if MEM_SOURCE_FILL_MEMORY_DEBUG
	memset(ret, 0xAA, (sz+7) & ~(ssize_t)7);
#endif
	return ret;
}

void* MemSource::allocAligned(size_t align, size_t sz) const {
	void *ret = _msource->alloc_align(align, sz);
#if MEM_SOURCE_FILL_MEMORY_DEBUG
	memset(ret, 0xAA, (sz+7) & ~(ssize_t)7);
#endif
	return ret;
}

void MemSource::free(void *p) {
	if (p == nullptr) return;
#if MEM_SOURCE_FILL_MEMORY_DEBUG
	memset(p, 0xBB, allocatedSize(p));
#endif
	msource::MemSourceImpl::free(p);
}

size_t MemSource::allocatedSize(void *p) {
	return msource::MemSourceImpl::get_block_size(p);
}

int MemSource::getPhysicalNode() const {
	return _msource->get_node();
}

size_t MemSource::migrate(int phys_node) const {
	return _msource->migrate(phys_node);
}

Node MemSource::getNodeOf(void *p) {
	const NodeList& all = NodeList::allNodes();
	int nodeid = numa::msource::MemSourceImpl::physicalNodeOf(p);
	for (const Node &n : all)
		if (n.physicalId() == nodeid)
			return n;
	return Node();
}

Node MemSource::getLogicalNode() const {
	const NodeList& all = NodeList::allNodes();
	for (const Node &n : all)
		if (n.physicalId() == _msource->get_node())
			return n;
	return Node();
}

std::string MemSource::getDescription() const {
	char buff[4096];
	_msource->getDescription(buff, 4096);
	return std::string(buff);
}

struct msource_info MemSource::stats() const {
	return _msource->stats();
}

size_t MemSource::prefault(size_t bytes) {
	return _msource->prefault(bytes);
}

}

/*

extern "C" msource_t msource_create(int node, size_t sz, const char *str)
{
	return MemSource::create(node, sz, str);
}

extern "C" msource_t msource_global()
{
	return (msource_t) MemSource::global();
}

extern "C" void msource_destroy(msource_t s)
{
	if (!s) return;
	MemSource::destroy((MemSource*) s);
}

extern "C" void msource_abandon(msource_t s)
{
	MemSource::abandon((MemSource*) s);
}

extern "C" void msource_attach_data(msource_t s, void *data)
{
	((MemSource*) s)->attach_data(data);
}

extern "C" void *msource_get_data(msource_t s)
{
	return ((MemSource*) s)->get_data();
}

extern "C" void *msource_get_data_for_mem(void *p)
{
	return MemSource::get_data_for_mem(p);
}

extern "C" int msource_migrate(msource_t s, int node)
{
	((MemSource*) s)->migrate(node);
	return 0;
}

extern "C" int msource_get_node(msource_t s)
{
	return ((MemSource*) s)->get_node();
}

extern "C" struct msource_info msource_stats(msource_t s)
{
	return ((MemSource*) s)->stats();
}

extern "C" void *msource_alloc(msource_t s, size_t bytes)
{
	return ((MemSource*) s)->alloc(bytes);
}

extern "C" void msource_free(void *p)
{
	MemSource::free(p);
}

*/
