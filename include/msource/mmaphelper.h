#ifndef __MMAP_HELPER_H__
#define __MMAP_HELPER_H__

#include <sys/mman.h>    /* for mmap */
#include <stddef.h>
#include "base/node.hpp"
#include "msource/hpinuma_msource_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * Allocate sz bytes from system. If node >= 0, bind them to the given NUMA node
 */
HPINUMA_MSOURCE_EXPORT void *callMmap(size_t sz, int node);

/**
 * Bind memory region to given NUMA node. Returns mbind() return value.
 */
HPINUMA_MSOURCE_EXPORT int bindMemory(void *p, size_t sz, int node);

/**
 * Return NUMA node location of data pointed to by ptr, as returned by move_pages
 */
HPINUMA_MSOURCE_EXPORT int getNumaNodeForMemory(const void *ptr);

/**
 * Stores NUMA node locations of data pointed to by ptr into results. Returns 
 * return value of move_pages()
 */
HPINUMA_MSOURCE_EXPORT int getNumaNodeForMemory_n(size_t count, const void **ptrs, int *results);

/**
 * Move memory region to given node
 */
HPINUMA_MSOURCE_EXPORT int moveMemory(void *p, size_t sz, int node);

/**
 * Touches every page from given memory region, to make it page-fault into working set
 */
HPINUMA_MSOURCE_EXPORT void touchMemory(void *p, size_t sz);


#ifdef __cplusplus
}  /* extern "C" */

#include <new>
#include <utility>
#include <vector>


namespace numa {
namespace util {
	using ::callMmap;
	using ::bindMemory;
	using ::munmap;
	
	using ::getNumaNodeForMemory;
	
	/**
	 * Return NUMA node locations of data pointed to by ptr, as returned by move_pages
	 */
	template <class T>
	inline std::vector<int> getNumaNodeForMemory(const std::vector<T*> &ptrs) {
		std::vector<int> result(ptrs.size());
		if (getNumaNodeForMemory_n(ptrs.size(), (void**) &ptrs[0], &result[0]) != 0) {
			for (auto &i : result) i = -1;
		}
		return result;
	}

	/**
	 * Get NUMA node locations for given chunk of memory
	 */
	HPINUMA_MSOURCE_EXPORT std::vector<int> getNumaNodeForMemory(const void *p, size_t sz);
	
	/**
	 * Memory allocator that uses mmap() directly
	 */
	template <class T>
	class MmapAllocator
	{
	public:
		numa::Node node;
		
		typedef T*                  pointer;
		typedef const T*            const_pointer;
		typedef void*               void_pointer;
		typedef const void*         const_void_pointer;
		typedef T                   value_type;
		typedef size_t              size_type;
		typedef ptrdiff_t           difference_type;
		typedef T&                  reference;
		typedef const T&            const_reference;
	
		template <class U>
		struct rebind {
			typedef MmapAllocator<U> other;
		};

	public:
		MmapAllocator(const numa::Node &n = numa::Node()) : node(n) {}
		template <class U> MmapAllocator(const MmapAllocator<U> &other) : node(other.node) {}
		template <class U> MmapAllocator(MmapAllocator<U> &&other) : node(other.node) {}
		~MmapAllocator() {}
	
		template <class U> MmapAllocator& operator=(const MmapAllocator<U>& other) {
			node = other.node;
		}
		template <class U> MmapAllocator& operator=(MmapAllocator<U>&& other) {
			node = other.node;
		}
	
	public:
		inline pointer allocate(size_type n) {
			return static_cast<pointer>(callMmap(sizeof(T) * n, node.physicalId()));
		}
	
		inline pointer allocate(pointer p, size_type n) {
			return static_cast<pointer>(callMmap(sizeof(T) * n, node.physicalId()));
		}
	
		inline void deallocate(pointer p, size_type n) {
			munmap(static_cast<void_pointer>(p), n);
		}
	
		template<typename... Args>
		inline void construct(pointer p, Args&&... args) {
			new (p) T(std::forward<Args>(args)...);
		}
	
		inline void destroy(pointer p) {
			p->~T();
		}
		
		template <class U> bool operator==(const MmapAllocator<U> &other) const { return true; }
		template <class U> bool operator!=(const MmapAllocator<U> &other) const { return false; }
	};
}
}

#endif

#endif /* __MMAP_HELPER_H__ */

