#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include <numa.h>

#include "PGASUS/base/node.hpp"
#include "base/debug.hpp"
#include "base/strutil.hpp"
#include "base/topology.hpp"

namespace {

using numa::util::Topology;

std::vector<int> get_sorted_elements_from_string(const std::string &s) {
	std::vector<int> ret;
	for (auto &part : numa::util::split(s, ',')) {
		auto nums = numa::util::split(part, '-');

		int a = -1, b = -1;
		if (nums.size() < 1 || sscanf(nums[0].c_str(), "%d", &a) == 0) {
			fprintf(stderr, "Malformed range string: '%s' from '%s'\n",
				part.c_str(), s.c_str());
			return {};
		}
		if (nums.size() == 2) {
			if (sscanf(nums[1].c_str(), "%d", &b) == 0) {
				fprintf(stderr, "Malformed range string: '%s' from '%s'\n",
					part.c_str(), s.c_str());
				return {};
			}
		} else {
			b = a;
		}

		for (int i = a; i <= b; i++)
			ret.push_back(i);
	}

	std::sort(ret.begin(), ret.end());

	return ret;
}

using PhysicalToLogicalNodeMapping = std::vector<int>;

PhysicalToLogicalNodeMapping getPhysicalToLogicalMapping() {
	const std::vector<int> & physicalIds = Topology::get()->node_ids();
	PhysicalToLogicalNodeMapping result;
	if (physicalIds.empty()) {
		fprintf(stderr, "Warning: no (NUMA) nodes detected?!");
		return result;
	}

	const int maxPhysId = physicalIds.back();
	// Allocate vector that is large enough to map from potential physical node
	// ids to:
	// 	-2 -> physical ID is not used in the system
	// 	-1 -> node with that ID is not used in the application
	// >=0 -> logical node ID within the application run
	result.resize(static_cast<size_t>(maxPhysId + 1), -2);

	auto setToUseAllNodes = [&result, &physicalIds] () {
		int logicalIndex = 0;
		for (const int physicalId : physicalIds) {
			result[static_cast<int>(physicalId)] = logicalIndex++;
		}
	};

	const char *envNodes = getenv("NUMA_NODES");

	// nothing specified -> return normal
	if (envNodes == nullptr) {
		setToUseAllNodes();
		return result;
	}

	// try to extract node ids from env string
	const std::vector<int> requestedPhysIDs =
		get_sorted_elements_from_string(envNodes);
	if (!requestedPhysIDs.empty()) do {
		if (requestedPhysIDs.back() > maxPhysId) {
			fprintf(stderr,
				"Requested invalid NUMA node ID, valid maximum ID is %i\n",
				maxPhysId);
			break;
		}
		// default: don't use node
		for (const int physicalId : physicalIds) {
			result[static_cast<int>(physicalId)] = -1;
		}
		// use only given nodes
		int nextLogicalID = 0;
		bool configOkay = true;
		for (const int n : requestedPhysIDs) {
			int & mappedLogicalID = result[static_cast<size_t>(n)];
			if (mappedLogicalID == -2) {
				fprintf(stderr, "Physical node with ID %i does not exist.\n", n);
				configOkay = false;
				break;
			}
			if (mappedLogicalID >= 0) {
				fprintf(stderr, "Warning: physical node with ID %i requested multiple times.\n", n);
				continue;
			}
			mappedLogicalID = nextLogicalID++;
		}
		if (configOkay) {
			// Configuration is fine, return it.
			return result;
		}
	} while(false);

	fprintf(stderr, "Invalid Node Configuration: \"%s\". Using all nodes.\n", envNodes);
	std::fill(result.begin(), result.end(), -2);
	setToUseAllNodes();
	return result;
}

// create/return static instance of node mapping
const std::vector<int>& physicalToLogical() {
	const static PhysicalToLogicalNodeMapping mapping = [] () {
		const auto mapping = getPhysicalToLogicalMapping();

#if ENABLE_DEBUG_LOG
		std::stringstream ss;
		ss << "Using Nodes: [ ";
		for (size_t i = 0; i < mapping.size(); i++) {
			if (mapping[i] >= 0)
				ss << i << " ";
		}
		ss << "] (set NUMA_NODES environment variable to change)";
		numa::debug::log(numa::debug::DEBUG, "%s", ss.str().c_str());
#endif

		return mapping;
	}();

	return mapping;
}

// make sure this is allocated before multithreaded starts
const PhysicalToLogicalNodeMapping &s_mapping = physicalToLogical();

size_t evnThreadCount() {
	const size_t tc = [] () -> size_t {
		const char *envThreads = getenv("NUMA_THREADS");
		if (envThreads == nullptr) return 0u;
		size_t envTc = 0u;
		if (sscanf(envThreads, "%zu", &envTc) != 1) {
			fprintf(stderr, "Invalid value for thread count in NUMA_THREADS "
				"environment variable (%s). Using all available threads "
				"instead.", envThreads);
			return 0u;
		}
		return envTc;
	}();
	return tc;
}

const std::vector<size_t>& logicalNodeThreadCounts() {
	static const std::vector<size_t> tcs = [] () {
		const numa::NodeList& nodes = numa::NodeList::logicalNodes();
		std::vector<size_t> tcs(nodes.size(), 0);
		const size_t env = evnThreadCount();
		for (size_t n = 0u; n < nodes.size(); ++n) {
			const numa::Node& node = nodes[n];
			const size_t hwThreads = Topology::get()->get_node(
				node.physicalId())->cpus.size();
			tcs[n] = env == 0u ? hwThreads : std::min(env, hwThreads);
		}
		return tcs;
	}();
	return tcs;
}

} // anonymous namespace


namespace numa {

Node Node::curr() {
	const int physId = Topology::get()->curr_numa_node()->id;
	const PhysicalToLogicalNodeMapping &mapping = physicalToLogical();
	assert(physId >= 0 && physId < static_cast<int>(mapping.size()));

	Node result;
	result._physical_id = physId;
	result._logical_id = mapping[physId];
	assert(result._logical_id >= 0);

	return result;
}

Node Node::forCpuid(CpuId id) {
	int node_id = Topology::get()->node_of_cpuid(id)->id;
	const PhysicalToLogicalNodeMapping &mapping = physicalToLogical();

	Node result;
	result._physical_id = node_id;
	result._logical_id = mapping[node_id];

	return result;
}

CpuId Node::currCpuid() {
	return util::Topology::curr_cpu_id();
}

size_t Node::cpuCount() const {
	return util::Topology::get()->get_node(physicalId())->cpus.size();
}

int Node::indexOfCpuid(CpuId id) const {
	return util::Topology::get()->get_node(physicalId())->core_of(id);
}

const std::vector<CpuId>& Node::cpuids() const {
	return util::Topology::get()->get_node(physicalId())->cpus;
}

size_t Node::threadCount() const {
	if (!valid()) {
		return 0u;
	}
	return logicalNodeThreadCounts()[static_cast<size_t>(_logical_id)];
}

size_t Node::memorySize() const {
	return util::Topology::get()->get_node(physicalId())->memorySize;
}

size_t Node::freeMemory() const {
	long long freeMem = -1;
	numa_node_size64(physicalId(), &freeMem);
	return static_cast<size_t>(freeMem > 0 ? freeMem : 0);
}

NodeList Node::nearestNeighbors(const size_t maxCount, const bool withCPUsOnly) const {
	NodeList neighbors;
	if (!valid()) {
		assert(false);
		return neighbors;
	}

	const Topology::NumaNode *physNode = Topology::get()->get_node(_physical_id);
	if (!physNode) {
		assert(false);
		return neighbors;
	}

	const PhysicalToLogicalNodeMapping &mapping = physicalToLogical();
	const NodeList &logicalNodes = NodeList::logicalNodes();

	for (const std::pair<int, Topology::NumaNode*> &distance_physId : physNode->nearestNeighbors) {
		if (neighbors.size() == maxCount) {
			break;
		}
		const size_t logicalNeighborId =
			static_cast<size_t>(mapping[distance_physId.second->id]);
		assert(logicalNeighborId < logicalNodes.size());
		const Node &neighbor = logicalNodes[logicalNeighborId];
		if (withCPUsOnly && neighbor.cpuCount() == 0) {
			continue;
		}
		neighbors.push_back(neighbor);
	}

	return neighbors;
}

NodeList Node::nearestNeighborsWithCPUs(const size_t maxCount) const {
	return nearestNeighbors(maxCount, true);
}

const NodeList& NodeList::logicalNodes() {
	const static NodeList list = [] () {
		NodeList result;
		const PhysicalToLogicalNodeMapping &mapping = physicalToLogical();

		for (size_t i = 0; i < mapping.size(); i++) {
			if (mapping[i] >= 0) {
				Node node;
				node._physical_id = i;
				node._logical_id = mapping[i];
				result.push_back(node);
			}
		}

		// physical node IDs and mapping should be sorted already
		assert(std::is_sorted(result.begin(), result.end(),
			[] (const Node &lhs, const Node &rhs) -> bool {
				return lhs._logical_id < rhs._logical_id; }));

		return result;
	}();
	return list;
}

size_t NodeList::logicalNodesCount() {
	return logicalNodes().size();
}

const NodeList& NodeList::logicalNodesWithCPUs() {
	const static NodeList list = [] () {
		NodeList result;
		const NodeList &allNodes = logicalNodes();
		for (const Node &node : allNodes) {
			if (node.cpuCount() > 0) {
				result.push_back(node);
			}
		}
		return result;
	}();
	return list;
}

size_t NodeList::logicalNodesWithCPUsCount() {
	return logicalNodesWithCPUs().size();
}

int NodeList::physicalToLogicalId(const int physicalId) {
	const PhysicalToLogicalNodeMapping & mapping = physicalToLogical();
	const size_t physUnsigned = static_cast<size_t>(physicalId);
	if (physicalId < 0 || physUnsigned >= mapping.size()) {
		return -1;
	}
	return mapping[physUnsigned];	
}

}
