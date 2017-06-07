#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>

#include <sched.h>
#include <hwloc.h>

#include "util/topology.hpp"


/* There seems to be some confusion about naming hwloc types */
#ifndef HWLOC_OBJ_NUMANODE
#define HWLOC_OBJ_NUMANODE HWLOC_OBJ_NODE
#endif


template <class T>
inline static T& insert_into_vector(std::vector<T> &vec, size_t idx, const T& val) {
	vec.resize(std::max(idx+1, vec.size()));
	vec[idx] = val;
	return vec[idx];
}


namespace numa {
namespace util {

int Topology::NumaNode::core_of(const int cpuid) const {
	const auto it = std::find(cpus.begin(), cpus.end(), cpuid);
	if (it == cpus.end()) {
		return -1;
	}
	return it - cpus.begin();
}

const Topology* Topology::get() {
	static const Topology s_instance;
	return &s_instance;
}

Topology::Topology() {
	if (hwloc_topology_init(&_topology) != 0) {
		throw std::runtime_error("PGASUS Topology: Initializing hwloc failed!");
	}
	// TODO distance matrix?
	if (hwloc_topology_load(_topology) != 0) {
		throw std::runtime_error("PGASUS Topology: Loading hwloc topology failed!");
	}
	
	// get ptr to first numa node
	const int numa_depth = hwloc_get_type_or_below_depth(_topology, HWLOC_OBJ_NUMANODE);
	if (numa_depth == HWLOC_TYPE_DEPTH_MULTIPLE) {
		throw std::runtime_error("PGASUS Topology: Hwloc reports NUMA nodes at "
			"multiple depths. This is not supported.");
	}
	hwloc_obj_t node = hwloc_get_obj_by_depth(_topology, numa_depth, 0);

	_total_cpu_count = 0;
	
	// iterate through NUMA nodes
	while (node != nullptr) {
		const int node_id = node->os_index;
		if (node_id < 0) {
			throw std::runtime_error("PGASUS Topology: Unexpected negative NUMA "
				"node id: " + std::to_string(node_id));
		}

		_nodeIds.push_back(node_id);
		
		// create numa node struct
		NumaNode *node_obj = insert_into_vector(_nodes, node_id, new NumaNode());
		node_obj->id = node_id;
		
		// assign CPUs from this node's CPU-set
		unsigned cpu_id;
		hwloc_bitmap_foreach_begin(cpu_id, node->cpuset) {
			node_obj->cpus.push_back(cpu_id);
			insert_into_vector(_cpu_to_node, cpu_id, node_obj);
			_total_cpu_count++;
		}
		hwloc_bitmap_foreach_end();
		
		node = node->next_cousin;
	}

	// hwloc reported node IDs are not always sorted, but a sorted index list is
	// easier to handle at other places in the library.
	std::sort(_nodeIds.begin(), _nodeIds.end());

	// assign topology distances
	for (const int nodeId : _nodeIds) {
		NumaNode* node = _nodes[static_cast<size_t>(nodeId)];
		assert(node);
		// distances to non-assigned nodes ids will be marked with -1
		node->distances.resize(_nodes.size(), -1);
		
		std::string node_file_name = "/sys/devices/system/node/node";
		node_file_name += std::to_string(nodeId);
		node_file_name += "/distance";
		std::fstream node_file(node_file_name, std::ios_base::in);

		if (!node_file.good()) {
			fprintf(stderr,
				"PGASUS Topology init: node file not readable (%s).\n",
				node_file_name.c_str());
			continue;
		}

		for (const int cousin : _nodeIds) {
			int distance;
			node_file >> distance;
			if (!node_file) {
				fprintf(stderr, "PGASUS Topology init: could not read node "
					"distance %i->%i from %s\n", nodeId, cousin,
					node_file_name.c_str());
				continue;
			}
			node->distances[static_cast<size_t>(cousin)] = distance;
			// Insert into neighbor list, yet unordered.
			node->nearestNeighbors.emplace_back(distance,
				_nodes[static_cast<size_t>(cousin)]);
		}
		std::sort(node->nearestNeighbors.begin(), node->nearestNeighbors.end(),
			[] (const std::pair<int, NumaNode*> &lhs,
				const std::pair<int, NumaNode*> &rhs) -> bool {
				return lhs.first < rhs.first
					|| (lhs.first == rhs.first
						&& lhs.second->id < rhs.second->id);
			});
	}
}

Topology::~Topology() {
	hwloc_topology_destroy(_topology);
}


int Topology::curr_cpu_id() {
	return sched_getcpu();
}

const std::vector<int> & Topology::node_ids() const {
	return _nodeIds;
}

size_t Topology::number_of_nodes() const {
	return _nodeIds.size();
}

size_t Topology::total_cpu_count() const {
	return _total_cpu_count;
}

const Topology::NumaNode* Topology::get_node(int n) const {
	assert(n >= 0 && size_t(n) < _nodes.size());
	return _nodes[n];
}

const Topology::NumaNode* Topology::node_of_cpuid(int cpu) const {
	assert (cpu >= 0 && size_t(cpu) < _cpu_to_node.size());
	return _cpu_to_node[cpu];
}

const Topology::NumaNode* Topology::curr_numa_node() const {
	return node_of_cpuid(curr_cpu_id());
}

int Topology::cores_on_node(int n) const {
	const NumaNode * node = get_node(n);
	return node ? node->cpus.size() : -1;
}

int Topology::core_of_cpuid(int cpu) const {	// return on-chip core no.
	return node_of_cpuid(cpu)->core_of(cpu);
}

void Topology::print(std::ostream &stream) const {
	stream << "Total number of CPUs: " << _total_cpu_count << std::endl;
	for (const int nodeId : _nodeIds) {
		const NumaNode * node = _nodes[static_cast<size_t>(nodeId)];
		
		stream << "Node [" << nodeId << "]" << std::endl;
		stream << "\tCPUs: [ ";
		for (const int cpu : node->cpus) stream << cpu << " ";
		stream << "]" << std::endl;
		stream << "\tNearest Neighbors: ";
		for (const auto &dn : node->nearestNeighbors) {
			stream << '(' << dn.first << ", " << dn.second->id << ") ";
		}
		stream << std::endl;
	}

	stream << "# Distance matrix:" << std::endl;
	stream << "     ";
	for (const int nodeX : _nodeIds) {
		stream << std::setw(4) << nodeX;
	}
	stream << std::endl;
	for (const int nodeY : _nodeIds) {
		stream << std::setw(4) << nodeY << " ";
		for (const int nodeX : _nodeIds) {
			stream << std::setw(4) << _nodes[nodeX]->distances[nodeY];
		}
		stream << std::endl;
	}
}

}
}
