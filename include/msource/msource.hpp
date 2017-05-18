#pragma once

#include <string>
#include "base/node.hpp"

namespace numa {

namespace msource {
class MemSourceImpl;
}

struct msource_info
{
	size_t hugeobj_count;
	size_t hugeobj_used;
	size_t hugeobj_size;
	
	size_t arena_count;
	size_t arena_used;
	size_t arena_size;
};

static constexpr size_t MEM_PAGE_SIZE = 4096;

class MemSource
{
private:
	msource::MemSourceImpl *_msource;
	static Node getNodeOf(void *p);
	
	MemSource(msource::MemSourceImpl *ms);

public:
	MemSource();
	MemSource(const MemSource &other);
	MemSource(MemSource &&other);
	~MemSource();

	MemSource& operator=(const MemSource &other);
	MemSource& operator=(MemSource &&other);

	bool operator==(const MemSource &other) const { return _msource == other._msource; }
	bool operator!=(const MemSource &other) const { return _msource != other._msource; }

	static MemSource create(int phys_node, size_t sz, const char *str, size_t mmap_threshold = 1LL<<24, int phys_home_node = -1);
	static const MemSource& global();
	static const MemSource& forNode(size_t phys_node);

	void* alloc(size_t sz) const;
	void* allocAligned(size_t align, size_t sz) const;
	static void free(void *p);
	static size_t allocatedSize(void *p);

	template <class T, class ... Args>
	inline T* construct(const Args&... args) const {
		return new (alloc(sizeof(T))) T(args...);
	}

	template <class T, class ... Args>
	inline T* construct(Args&&... args) const {
		return new (alloc(sizeof(T))) T(std::forward<Args>(args)...);
	}

	template <class T>
	inline T* construct() const {
		return new (alloc(sizeof(T))) T();
	}

	template <class T>
	static void destruct(T* &ptr) {
		if (ptr != nullptr) {
			ptr->~T();
			free((void*)ptr);
			ptr = nullptr;
		}
	}

	template <class T>
	static void destructNoRef(T* ptr) {
		if (ptr != nullptr) {
			ptr->~T();
			free((void*)ptr);
		}
	}

	int getPhysicalNode() const;
	Node getLogicalNode() const;
	size_t migrate(int phys_dst) const;

	template <class T> static Node nodeOf(T *p) { return getNodeOf((void*) p); }

	std::string getDescription() const;
	struct msource_info stats() const;

	bool valid() const { return _msource != nullptr; }

	static MemSource create(Node node, size_t sz, const char *str, const Node& home_node = Node()) {
		return create(node.physicalId(), sz, str, home_node.physicalId());
	}
	static MemSource forNode(const Node &node) {
		return forNode(node.physicalId());
	}
	size_t migrate(const Node& node) const {
		return migrate(node.physicalId());
	}

	size_t prefault(size_t bytes);
};

}
