#pragma once


#include <atomic>


/**
 * Lock-free hash table implementation.
 *
 * 
 * Use a lazily-growing, cont.storage-based vector<T>-alike allocator storage
 * to store the Entries, OR just allocate them using the normal allocator.
 *
 * How do we do the resize operation? We may just do a RW-lock around the 
 * access to the entries array. This lock just guards resizing. (read=noresize,
 * write=resize). For each put we check whether the current load factor needs
 * resizing, make sure only one threads will do the actual resize operation,
 * let it wait for the RW lock, and having exclusive access, do the operation.
 * As long as there are no resizes needed, this is "almost" lock-free.
 *
 * And, still, we need to care about lock-free problems. When N threads want
 * to update map[K], they will each overwrite previous values, making them dangling
 * so we need measures to observe object usage
 */
 
template <class Key, class T, class Hash = std::hash<K>>
class HashTable_lockfree
{
	struct Entry {
		Key key;
		T value;
	};
	
	size_t						curr_objs;
	size_t						max_objs;
	std::vector<Entry*>			ptr_to_entry;
	
	void put()
};
