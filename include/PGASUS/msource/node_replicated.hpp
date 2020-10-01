#pragma once

#include <vector>
#include <mutex>
#include <cassert>
#include <cstring>

#include "PGASUS/base/node.hpp"
#include "PGASUS/msource/msource.hpp"
#include "PGASUS/base/spinlock.hpp"

namespace numa {


/**
 * Control all global instances of given class type, indexable by int.
 */
template <class T, class LockType = numa::SpinLock>
class NodeReplicated
{
	private:
		LockType                _global_lock;
		size_t                  _logical_node_count = 0;
		T**                     _global_data = nullptr;
	
	public:
		NodeReplicated() {
			_logical_node_count = NodeList::logicalNodesCount();
			_global_data = (T**) MemSource::global().allocAligned(
				64, _logical_node_count * sizeof(T*));
			
			// zero out
			for (size_t i = 0; i < _logical_node_count; i++)
				_global_data[i] = nullptr;
		}
		
		~NodeReplicated() {
			std::lock_guard<LockType> lock(_global_lock);
			
			// Destroy NodeLocalStorage instances + return mem
			for (size_t i = 0; i < _logical_node_count; i++) {
				if (_global_data[i] != nullptr) {
					_global_data[i]->~T();
					MemSource::free((void*) _global_data[i]);
				}
			}
			MemSource::free((void*) _global_data);
		}
		
		T &get(const Node& node) {
			assert(node.valid());
			size_t n = node.logicalId();
			assert(n < _logical_node_count);
			
			if (_global_data[n] == nullptr) {
				// make sure to allocate only once
				std::lock_guard<LockType> lock(_global_lock);
				if (_global_data[n] == nullptr) {
					_global_data[n] = new (MemSource::forNode(
						node.physicalId()).allocAligned(64, sizeof(T))) T(node);
				}
			}
			
			return *_global_data[n];
		}
		
		std::vector<T*> get_all_registered() {
			std::lock_guard<LockType> lock(_global_lock);
			
			std::vector<T*> ret;
			ret.reserve(_logical_node_count);
			
			for (size_t i = 0; i < _logical_node_count; i++) {
				if (_global_data[i] != nullptr) 
					ret.push_back(_global_data[i]);
			}
			
			return std::move(ret);
		}
};


}
