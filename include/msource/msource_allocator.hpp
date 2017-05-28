#pragma once

#include "msource/msource.hpp"

namespace numa {

template <class T>
class MemSourceAllocator
{
private:
	MemSource ms;

public:
	typedef T*                  pointer;
	typedef const T*            const_pointer;
	typedef void*               void_pointer;
	typedef const void*         const_void_pointer;
	typedef T                   value_type;
	typedef size_t              size_type;
	typedef ssize_t             difference_type;
	typedef T&                  reference;
	typedef const T&            const_reference;
	
	template <class U>
	struct rebind {
		typedef MemSourceAllocator<U> other;
	};

	template <class U>
	friend class MemSourceAllocator;

public:
	MemSourceAllocator(const MemSource &m) : ms(m) {}
	template <class U> MemSourceAllocator(const MemSourceAllocator<U> &other) : ms(other.ms) {}
	template <class U> MemSourceAllocator(MemSourceAllocator<U> &&other) : ms(other.ms) {}
	
	~MemSourceAllocator() {}
	
	template <class U> void operator=(const MemSourceAllocator<U>& other) { ms = other.ms; }
	template <class U> void operator=(MemSourceAllocator<U>&& other) { ms = other.ms; }
	
public:
	inline const MemSource& msource() const { return ms; }
	
	inline pointer allocate(size_type n) {
		return static_cast<pointer>(ms.alloc(sizeof(T) * n));
	}
	
	inline pointer allocate(pointer p, size_type n) {
		return static_cast<pointer>(ms.alloc(sizeof(T) * n));
	}
	
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wunused-parameter"
	inline void deallocate(pointer p, size_type n) {
		ms.free(static_cast<void_pointer>(p));
	}
	#pragma GCC diagnostic pop
	
	template<typename... Args>
	inline void construct(pointer p, Args&&... args) {
		new (p) T(std::forward<Args>(args)...);
	}
	
	inline void destroy(pointer p) {
		p->~T();
	}
	
	template <class T2>
	inline bool operator==(const MemSourceAllocator<T2> &a2) const {
		return msource() == a2.msource();
	}
	
	template <class T2>
	inline bool operator!=(const MemSourceAllocator<T2> &a2) const {
		return msource() != a2.msource();
	}
};

}
