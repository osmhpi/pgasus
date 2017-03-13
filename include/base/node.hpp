#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace numa {

class Node;

class NodeList : public std::vector<Node> {
	static NodeList createAllNodesList();
public:
	static const NodeList& allNodes();
	static size_t allNodesCount();
	static size_t physicalNodesCount();
};

typedef size_t CpuId;
typedef std::vector<CpuId> CpuSet;

class Node {
private:
	int16_t _index;
	int16_t _physical_node;
	friend class NodeList;

public:
	Node() : _index(-1), _physical_node(-1) {}

	inline bool operator==(const Node &other) const {
		return (_index == other._index) && (_physical_node == other._physical_node);
	}
	inline bool operator!=(const Node &other) const {
		return !(*this == other);
	}

	inline bool operator<(const Node &other) const {
		return _physical_node < other._physical_node;
	}
	
	static Node curr();
	static Node forCpuid(CpuId id);
	
	static CpuId currCpuid();

	std::vector<CpuId> cpuids() const;
	
	inline bool valid() const {
		return _index >= 0 && _physical_node >= 0;
	}

	int physicalId() const {
		return _physical_node;
	}

	int logicalId() const {
		return _index;
	}

	size_t cpuCount() const;
	size_t threadCount() const;
	int indexOfCpuid(CpuId id) const;

	NodeList nearestNeighbors(size_t count = (size_t)-1 /* all */) const;
};

}
