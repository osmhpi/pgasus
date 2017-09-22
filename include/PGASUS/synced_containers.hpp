#pragma once

#include <algorithm>
#include <cassert>
#include <deque>
#include <mutex>
#include <vector>

#include <semaphore.h>

#include "PGASUS/base/spinlock.hpp"

namespace numa {
namespace util {

template <class T, class Allocator = std::allocator<T>>
class SyncDeque {
private:
	typedef std::deque<T,Allocator> RawType;
	typedef numa::SpinLock Lock;

private:
	RawType             _container;
	Lock                _mutex;

public:
	explicit SyncDeque(const Allocator& alloc = Allocator())
		: _container(alloc)
	{
	}
	
	inline T& front() { 
		std::lock_guard<Lock> lock(_mutex);
		return _container.front();
	}
	
	inline T& back() { 
		std::lock_guard<Lock> lock(_mutex);
		return _container.back();
	}
	
	inline void push_front(const T &v) { 
		std::lock_guard<Lock> lock(_mutex);
		_container.push_front(v);
	}
	
	inline void push_back(const T &v) {
		std::lock_guard<Lock> lock(_mutex);
		_container.push_back(v);
	}
	
	inline bool try_pop_front(T &v) {
		std::lock_guard<Lock> lock(_mutex);
		if (_container.empty())
			return false;
		v = _container.front();
		_container.pop_front();
		return true;
	}
	
	inline int64_t mutex_count() const { return _mutex.count(); }
};

/**
 * Synchronized prioritized list of pointers, sorted by their natural order
 */
template <class T, class Allocator = std::allocator<T*>>
class SortedSyncPtrVector
{
private:
	typedef std::vector<T*,Allocator> VectorType;
	typedef numa::SpinLock Lock;
	
	VectorType              _data;
	Lock                    _lock;
	sem_t                   _count;
	
	bool                    _delete_data;
	
	struct Comparator {
		inline bool operator()(const T* &a, const T* &b) const {
			return *a < *b;
		}
	};
	
public:
	SortedSyncPtrVector(bool del, size_t initial = 64, const Allocator &alloc = Allocator())
		: _data(alloc)
		, _delete_data(del)
	{
		_data.reserve(initial);
		if (sem_init(&_count, 0, 0) != 0) {
			assert(false);
		}
	}
	
	~SortedSyncPtrVector() {
		if (_delete_data) {
			for (T *t : _data) delete t;
		}
		if (sem_destroy(&_count) != 0) {
			assert(false);
		}
	}
	
	void put(T *t) {
		std::lock_guard<Lock> l(_lock);
		
		_data.push_back(t);
		std::sort(_data.begin(), _data.end(), Comparator());
		
		if (sem_post(&_count) != 0) {
			assert(false);
		}
	}
	
	bool try_get(T* &result) {
		if (sem_trywait(&_count) == 0) {
			std::lock_guard<Lock> l(_lock);
		
			result = _data.back();
			_data.pop_back();
			return true;
		}
		return false;
	}
};

}
}
