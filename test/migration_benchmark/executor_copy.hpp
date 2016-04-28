#pragma once 

#include "executor.hpp"

#include <numaif.h>



template <class T>
struct CopyExecutor : public AbstractExecutor<CopyExecutor<T>, T> {
	typedef std::vector<T, numa::util::MmapAllocator<T>> CollectionType;
	typedef CollectionType* Collection;
	typedef AbstractExecutor<CopyExecutor<T>, T> BaseClass;
	
	inline BaseClass* thisBase() { 
		return static_cast<BaseClass*>(this);
	}
	
	CopyExecutor() {
		static_assert(std::is_copy_constructible<T>::value, "CopyExecutor: Must be copy-constructible!");
	}
	
 	Collection* generate() {
 		CollectionType *result = new CollectionType(numa::util::MmapAllocator<T>(thisBase()->from()));
 		result->reserve(thisBase()->elements());
 		for (int i = 0; i < thisBase()->elements(); i++) {
 			result->push_back(thisBase()->construct());
 		}
 		return new Collection(result);
 	}
 	
 	std::vector<const T*> items(const Collection *c) {
 		std::vector<const T*> result;
 		result.reserve((*c)->size());
 		for (auto &item : **c)
 			result.push_back(&item);
 		return result;
 	}
 	
 	void migrate(Collection *c) {
 		// build new collection, with newly constructed members
 		CollectionType *newcol = new CollectionType(numa::util::MmapAllocator<T>(thisBase()->to()));
 		CollectionType &old = **c;
 		newcol->reserve(old.size());
 		
		for (size_t i = 0; i < old.size(); i++) {
 			newcol->push_back(T(old[i]));
 		}
 		
 		delete *c;
 		*c = newcol;
 	}
 	
	numa::Node migrationNode() {
 		return thisBase()->to();
 	}
	
};
