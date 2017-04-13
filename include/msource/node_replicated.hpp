#pragma once

#include <vector>
#include <mutex>
#include <cassert>
#include <cstring>

#include "base/node.hpp"
#include "msource/msource.hpp"
#include "base/spinlock.hpp"

namespace numa {


/**
 * Control all global instances of given class type, indexable by int.
 */
template <class T, class LockType = numa::SpinLock>
class NodeReplicated
{
	private:
		LockType                _global_lock;
		size_t                  _count = 0;
		T**                     _global_data = nullptr;
	
	public:
		NodeReplicated() {
			_count = NodeList::allNodesCount();
			_global_data = (T**) MemSource::global().allocAligned(64, _count * sizeof(T*));
			
			// zero out
			for (size_t i = 0; i < _count; i++)
				_global_data[i] = nullptr;
		}
		
		~NodeReplicated() {
			std::lock_guard<LockType> lock(_global_lock);
			
			// Destroy NodeLocalStorage instances + return mem
			for (size_t i = 0; i < _count; i++) {
				if (_global_data[i] != nullptr) {
					_global_data[i]->~T();
					MemSource::free((void*) _global_data[i]);
				}
			}
			MemSource::free((void*) _global_data);
		}
		
		T &get(Node node) {
			assert(node.valid());
			size_t n = node.logicalId();
			assert(n < _count);
			
			if (_global_data[n] == nullptr) {
				// make sure to allocate only once
				std::lock_guard<LockType> lock(_global_lock);
				if (_global_data[n] == nullptr) {
					_global_data[n] = new (MemSource::forNode(node.physicalId()).allocAligned(64, sizeof(T))) T(node);
				}
			}
			
			return *_global_data[n];
		}
		
		std::vector<T*> get_all_registered() {
			std::lock_guard<LockType> lock(_global_lock);
			
			std::vector<T*> ret;
			ret.reserve(_count);
			
			for (size_t i = 0; i < _count; i++) {
				if (_global_data[i] != nullptr) 
					ret.push_back(_global_data[i]);
			}
			
			return std::move(ret);
		}
};


}
