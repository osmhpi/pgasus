#pragma once 

#include "executor.hpp"
#include "PGASUS/msource/msource.hpp"
#include "PGASUS/malloc.hpp"
#include "timer.hpp"

#include <numaif.h>


using numa::msource::MemSourceImpl;


template <class T>
struct MsourceExecutor : public AbstractExecutor<MsourceExecutor<T>, T> {
	struct Collection {
		std::vector<T> items;
		numa::MemSource msource;
	};
	
	typedef AbstractExecutor<MsourceExecutor<T>, T> BaseClass;
	
	inline BaseClass* thisBase() { 
		return static_cast<BaseClass*>(this);
	}
	
	MsourceExecutor() {
	}
	
 	Collection* generate() {
		numa::MemSource ms = numa::MemSource::create(thisBase()->from(), 1LL<<20, "ExecutorMsource");
		numa::PlaceGuard pg(ms);
 		
 		Collection *result = new Collection();
 		result->msource = ms;
 		result->items.reserve(thisBase()->elements());
 		
 		for (int i = 0; i < thisBase()->elements(); i++) {
 			result->items.push_back(thisBase()->construct());
 		}
 		return result;
 	}
 	
 	std::vector<const T*> items(const Collection *c) {
 		std::vector<const T*> result;
 		result.reserve(c->items.size());
 		for (auto &item : c->items)
 			result.push_back(&item);
 		return result;
 	}
 	
 	void migrate(Collection *c) {
		c->msource.migrate(thisBase()->to());
 	}
 	
	numa::Node migrationNode() {
 		return thisBase()->to();
 	}
	
};
