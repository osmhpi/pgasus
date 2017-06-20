#pragma once 

#include <algorithm>

#include <numaif.h>
#include <unistd.h>

#include "executor.hpp"
#include "msource/msource_allocator.hpp"
#include "msource/mmaphelper.h"


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
 		static const size_t PAGE_SIZE = sysconf(_SC_PAGESIZE);
 		static const uintptr_t PAGE_MASK = PAGE_SIZE - 1;
 		
 		// collect pages
 		const char *start = (char*) &c->front();
 		const char *end = (char*) &c->back() + sizeof(T) - 1;
 		uintptr_t lastPage = 0;
 		
 		std::vector<void*> pages;
 		pages.reserve((ssize_t)(end-start) / PAGE_SIZE + 1);
 		
 		for (const char *ptr = start; ptr <= end; ptr += sizeof(T)) {
 			uintptr_t page = (uintptr_t) ptr;
 			page &= ~PAGE_MASK;
 			if (page != lastPage) {
 				pages.push_back((void*) page);
 				lastPage = page;
 			}
 		}

		std::vector<int> dst(pages.size(), thisBase()->to().physicalId());
 		std::vector<int> statuses(pages.size(), -1);

		auto errorToString = [] (const int errorCode) {
			switch(errorCode) {
			case -EACCES:	return "-EACCES";
			case -EBUSY:	return "-EBUSY";
			case -EFAULT:	return "-EFAULT";
			case -EIO:		return "-EIO";
			case -ENOENT:	return "-ENOENT";
			case E2BIG:		return "E2BIG";
			case EACCES:	return "EACCES";
			case EFAULT:	return "EFAULT";
			case EINVAL:	return "EINVAL";
			case ENODEV:	return "ENODEV";
			case ENOENT:	return "ENOENT";
			case EPERM:		return "EPERM";
			case ESRCH:		return "ESRCH";
			default:		return "(Unkown error)";
			}
		};

		long long numBusyPages;
		int numMoveTries = 1;
		const int targetPhysId = thisBase()->to().physicalId();
		for (; numMoveTries <= 10; ++numMoveTries) {
			const int result =
				move_pages(0, pages.size(), &pages[0], &dst[0], &statuses[0], MPOL_MF_MOVE);

			if (result != 0) {
				fprintf(stderr, "move_pages failed with result %i\n", result);
				fprintf(stderr, "First page error: %i == %s\n",
					statuses[0], errorToString(statuses[0]));
				assert(false);
				return;
			}

			// If result==0, some pages can still be marked as -EBUSY
			// (happens at least on POWER).
			// Other kinds of errors are not expected here.
			numBusyPages = 0;
			for (const int status : statuses) {
				if (status == targetPhysId) {
					continue;
				}
				if (status == -EBUSY) {
					++numBusyPages;
					continue;
				}
				fprintf(stderr, "move_page failed with error: %i == %s\n",
					status, errorToString(status));
				return;
			}
			if (numBusyPages == 0) {
				break;
			}
		}
		/*if (numMoveTries > 1 && numBusyPages == 0) {
			fprintf(stderr, "Required %i tries for migrating busy pages\n",
				numMoveTries);
		}
		else */if (numBusyPages != 0) {
			fprintf(stderr,
				"Could not move busy pages within %i tries (still %lli busy pages)!\n",
				numMoveTries, numBusyPages);
			assert(false);
		}
 	}

	numa::Node migrationNode() {
 		return (_policy == PUSH) ? thisBase()->from() : thisBase()->to();
 	}

};
