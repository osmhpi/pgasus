#include "base/node.hpp"
#include "util/topology.hpp"
#include "util/strutil.hpp"
#include "util/debug.hpp"

#include <cstdlib>
#include <cstdio>
#include <vector>
#include <sstream>

#ifndef HPINUMA_MAX_NODES
#define HPINUMA_MAX_NODES 1024
#endif

static bool get_elements_from_string(const std::string &s, std::vector<int> &ret) {
	for (auto &part : numa::util::split(s, ',')) {
		auto nums = numa::util::split(part, '-');

		int a = -1, b = -1;
		if (nums.size() < 1 || sscanf(nums[0].c_str(), "%d", &a) == 0) {
			fprintf(stderr, "Malformed range string: '%s' from '%s'\n",
				part.c_str(), s.c_str());
			return false;
		}
		if (nums.size() == 2) {
			if (sscanf(nums[1].c_str(), "%d", &b) == 0) {
				fprintf(stderr, "Malformed range string: '%s' from '%s'\n",
					part.c_str(), s.c_str());
				return false;
			}
		} else {
			b = a;
		}

		for (int i = a; i <= b; i++)
			ret.push_back(i);
	}

	return true;
}

typedef std::vector<int> NodeMapping;

static NodeMapping getPhysicalToLogicalMapping() {
	int physNodeCount = numa::util::Topology::get()->max_node_id() + 1;
	NodeMapping result((size_t)physNodeCount, -1);
	std::vector<int> nodes;

	char *envNodes = getenv("NUMA_NODES");

	// nothing specified -> return normal
	if (envNodes == nullptr) goto setdefault;

	// try to exctract node ids from env string
	if (get_elements_from_string(envNodes, nodes) && !nodes.empty()) {
		// default: dont use node
		for (int &v : result) v = -1;

		// use only given nodes
		for (int &n : nodes) {
			if (n < 0 || n >= physNodeCount)
				goto fail;
			result[n] = 1;
		}

		// sort nodes
		int curr = 0;
		for (int &n : result) {
			if (n > 0) n = curr++;
		}

		goto out;
	}

fail:
	fprintf(stderr, "Invalid Node Configuration: \"%s\". Using all nodes.\n", envNodes);

setdefault:
	// normal mapping - use all nodes
	for (size_t i = 0; i < result.size(); i++)
		result[i] = i;

out:
	return result;
}

// create/return static instance of node mapping
static const std::vector<int>& physicalToLogical() {
	static NodeMapping mapping = getPhysicalToLogicalMapping();
	static bool first = true;
	if (first) {
		first = false;

		std::stringstream ss;
		ss << "Using Nodes: [ ";
		for (size_t i = 0; i < mapping.size(); i++) {
			if (mapping[i] >= 0)
				ss << i << " ";
		}
		ss << "] (set NUMA_NODES environment variable to change)";
		numa::debug::log(numa::debug::DEBUG, "%s", ss.str().c_str());
	}
	return mapping;
}

// make sure this is allocated before multithreaded starts
const NodeMapping &s_mapping = physicalToLogical();


namespace numa {

Node Node::curr() {
	int node_id = numa::util::Topology::get()->curr_numa_node()->id;
	const NodeMapping &mapping = physicalToLogical();

	Node result;
	result._physical_node = node_id;
	result._index = mapping[node_id];

	return result;
}

Node Node::forCpuid(CpuId id) {
	int node_id = numa::util::Topology::get()->node_of_cpuid(id)->id;
	const NodeMapping &mapping = physicalToLogical();

	Node result;
	result._physical_node = node_id;
	result._index = mapping[node_id];

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

std::vector<CpuId> Node::cpuids() const {
	return util::Topology::get()->get_node(physicalId())->cpus;
}

static size_t getThreadCount() {
	size_t envTc, ret = util::Topology::get()->get_node(0)->cpus.size();

	char *envThreads = getenv("NUMA_THREADS");
	if (envThreads == nullptr) return ret;
	if (sscanf(envThreads, "%zu", &envTc) != 1
			|| envTc > ret) {
		fprintf(stderr, "Invalid Thread Count: \"%s\". Using all cores per node.\n", envThreads);
		return ret;
	}
	return envTc;
}

size_t Node::threadCount() const {
	static size_t th = getThreadCount();
	return th;
}

NodeList Node::nearestNeighbors(size_t count) const {
	const NodeMapping &mapping = physicalToLogical();
	const NodeList &allNodes = NodeList::allNodes();

	count = std::min(count, allNodes.size() - 1);
	NodeList result;
	result.reserve(count);

	for (int idx = 1; idx < mapping.size() && result.size() < count; idx++) {
		int otherNode = (int)_physical_node ^ idx;
		if (mapping[otherNode]  > 0)
			result.push_back(allNodes[mapping[otherNode]]);
	}

	return result;
}

//
// All nodes
//
NodeList NodeList::createAllNodesList() {
	NodeList result;
	const NodeMapping &mapping = physicalToLogical();

	for (size_t i = 0; i < mapping.size(); i++) {
		if (mapping[i] >= 0) {
			Node node;
			node._physical_node = i;
			node._index = mapping[i];
			result.push_back(node);
		}
	}

	return result;
}

const NodeList& NodeList::allNodes() {
	static NodeList list = createAllNodesList();
	return list;
}

size_t NodeList::allNodesCount() {
	return allNodes().size();
}

}
