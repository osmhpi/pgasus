#pragma once

#include <cstdint>
#include <cstddef>
#include <limits>
#include <vector>

#include "PGASUS/base/hpinuma_base_export.h"

namespace numa {

class Node;

/**
 * Detect and list NUMA nodes configured for the application.
 *
 * This class assumes a static NUMA topology, so be careful when running on
 * dynamic POWER LPARs...
 */
class HPINUMA_BASE_EXPORT NodeList : public std::vector<Node> {
public:
	/**
	 * List of nodes available for the application.
	 * This can be adjusted by NUMA_NODES environment variable, which lists
	 * physical node IDs to be used. Otherwise, all physical nodes will be used.
	 * Logical node IDs in this list are always in consecutive order.
	 */
	static const NodeList& logicalNodes();
	static size_t logicalNodesCount();
	/* Same as logicalNodes() but excluding nodes with CPUs. */
	static const NodeList& logicalNodesWithCPUs();
	static size_t logicalNodesWithCPUsCount();
	static int physicalToLogicalId(int physicalId);
	/** Number of nodes actually available on the current hardware. */
	static size_t physicalNodesCount();
};

using CpuId = int;
using CpuSet = std::vector<CpuId>;

class HPINUMA_BASE_EXPORT Node {
private:
	int16_t _logical_id;
	int16_t _physical_id;
	friend class NodeList;

public:
	Node() : _logical_id(-1), _physical_id(-1) {}

	bool operator==(const Node &other) const {
		return (_logical_id == other._logical_id)
			&& (_physical_id == other._physical_id);
	}
	bool operator!=(const Node &other) const {
		return !(*this == other);
	}

	bool operator<(const Node &other) const {
		return _physical_id < other._physical_id;
	}
	
	static Node curr();
	static Node forCpuid(CpuId id);
	
	static CpuId currCpuid();

	const std::vector<CpuId>& cpuids() const;
	
	bool valid() const {
		return _logical_id >= 0 && _physical_id >= 0;
	}

	/** NUMA node ID defined by the hardware / operating system. */
	int physicalId() const {
		return _physical_id;
	}

	/** Logical ID of the node within an application instantiation. */
	int logicalId() const {
		return _logical_id;
	}

	size_t cpuCount() const;
	size_t threadCount() const;
	int indexOfCpuid(CpuId id) const;

	size_t memorySize() const;
	size_t freeMemory() const;

	NodeList nearestNeighbors(
		size_t maxCount = std::numeric_limits<size_t>::max(), /* all */
		bool withCPUsOnly = false) const;
	NodeList nearestNeighborsWithCPUs(
		size_t maxCount = std::numeric_limits<size_t>::max() /* all */) const;
};

}
