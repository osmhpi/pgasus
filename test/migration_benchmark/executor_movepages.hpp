#pragma once 

#include "executor.hpp"
#include "msource/msource_allocator.hpp"
#include "msource/mmaphelper.h"
#include <numaif.h>



template <class T>
struct MovePagesExecutor : public AbstractExecutor<MovePagesExecutor<T>, T> {
	MigrationPolicy _policy;
	
	typedef std::vector<T, numa::util::MmapAllocator<T>> Collection;
	typedef AbstractExecutor<MovePagesExecutor<T>, T> BaseClass;
	
	inline BaseClass* thisBase() { 
		return static_cast<BaseClass*>(this);
	}
	
	MovePagesExecutor(MigrationPolicy mp)
		: AbstractExecutor<MovePagesExecutor<T>,T>()
	{
		static_assert(std::is_pod<T>::value, "MovePages: POD only!");
		_policy = mp;
	}
	
 	Collection* generate() {
 		Collection *result = new Collection(numa::util::MmapAllocator<T>(thisBase()->from()));
 		result->reserve(thisBase()->elements());
 		for (int i = 0; i < thisBase()->elements(); i++) {
 			result->push_back(thisBase()->construct());
 		}
 		return result;
 	}
 	
 	std::vector<const T*> items(const Collection *c) {
 		std::vector<const T*> result;
 		result.reserve(c->size());
 		for (auto &item : *c)
 			result.push_back(&item);
 		return result;
 	}
 	
 	void migrate(Collection *c) {
 		constexpr static size_t PAGE_SIZE = 4096;
 		constexpr static uintptr_t PAGE_MASK = PAGE_SIZE - 1;
 		
 		// collect pages
 		char *start = (char*) &c->front();
 		char *end = (char*) &c->back() + sizeof(T) - 1;
 		uintptr_t lastPage = 0;
 		
 		std::vector<void*> pages;
 		pages.reserve((ssize_t)(end-start) / PAGE_SIZE + 1);
 		
 		for (char *ptr = start; ptr <= end; ptr += sizeof(T)) {
 			uintptr_t page = (uintptr_t) ptr;
 			page &= ~PAGE_MASK;
 			if (page != lastPage) {
 				pages.push_back((void*) page);
 				lastPage = page;
 			}
 		}
 		
		std::vector<int> dst(pages.size(), thisBase()->to().physicalId());
 		std::vector<int> status(pages.size(), -1);
 		
 		move_pages(0, pages.size(), &pages[0], &dst[0], &status[0], MPOL_MF_MOVE);
 		
 		for (size_t i = 0; i < status.size(); i++)
			assert(status[i] == thisBase()->to().physicalId());
 	}
 	
	numa::Node migrationNode() {
 		return (_policy == PUSH) ? thisBase()->from() : thisBase()->to();
 	}
	
};
