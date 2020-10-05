#pragma once

#include "PGASUS/base/ref_ptr.hpp"
#include "PGASUS/msource/PGASUS_msource_export.h"
#include "PGASUS/msource/msource.hpp"
#include "PGASUS/msource/msource_allocator.hpp"

#include <map>
#include <list>
#include <vector>


namespace numa {

/**
 * Some template aliases for common use
 */
template <class T>
using msvector = std::vector<T, MemSourceAllocator<T>>;

template <class T>
using mslist = std::list<T, MemSourceAllocator<T>>;

template <class K, class V, class Cmp = std::less<K>> 
using msmap = std::map<K, V, Cmp, MemSourceAllocator<std::pair<const K,V>>>;


class PGASUS_MSOURCE_EXPORT MemSourceReferenced : public numa::Referenced {
protected:
	virtual DeleteFunctor deleter() {
		return DeleteFunctor(MemSource::destruct<Referenced>);
	}
};

}
