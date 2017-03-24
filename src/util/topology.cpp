#include <algorithm>
#include <mutex>
#include <vector>
#include <fstream>

#include <sched.h>
#include <hwloc.h>

#include "util/topology.hpp"
#include "base/spinlock.hpp"


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

int Topology::NumaNode::core_of(int cpuid) const {
	for (size_t i = 0; i < cpus.size(); i++)
		if (cpuid == cpus[i])
			return i;
	return -1;
}

const Topology* Topology::get() {
	static Topology s_instance;
	return &s_instance;
}

Topology::Topology() {
	if (hwloc_topology_init(&_topology) != 0) {
		assert(false);
	}
	// TODO distance matrix?
	if (hwloc_topology_load(_topology) != 0) {
		assert(false);
	}
	
	// get ptr to first numa node
	int numa_depth = hwloc_get_type_or_below_depth(_topology, HWLOC_OBJ_NUMANODE);
	hwloc_obj_t node = hwloc_get_obj_by_depth(_topology, numa_depth, 0);
	
	// iterate through NUMA nodes
	while (node != nullptr) {
		int node_id = node->os_index;
		
		// create numa node struct
		NumaNode *node_obj = insert_into_vector(_nodes, node_id, new NumaNode());
		node_obj->id = node_id;
		
		// assign CPUs from this node's CPU-set
		unsigned cpu_id;
		hwloc_bitmap_foreach_begin(cpu_id, node->cpuset) {
			node_obj->cpus.push_back(cpu_id);
			insert_into_vector(_cpu_to_node, cpu_id, node_obj);
		}
		hwloc_bitmap_foreach_end();
		
		node = node->next_cousin;
	}
	
	// assign topology distances
	for (size_t idx = 0; idx < _nodes.size(); idx++) {
		_nodes[idx]->distances.resize(_nodes.size(), -1);
		
		std::string node_file_name = "/sys/devices/system/node/node";
		node_file_name += std::to_string(idx);
		node_file_name += "/distance";
		
		std::fstream node_file(node_file_name, std::ios_base::in);
		for (size_t k = 0; k < _nodes.size(); k++)
			node_file >> _nodes[idx]->distances[k];
		
		// fail?
		if (std::count(_nodes[idx]->distances.begin(), _nodes[idx]->distances.end(), -1) > 0) {
			fprintf(stderr, "Topology init: Invalid node distances read from %s\n", node_file_name.c_str());
		}
	}
}

Topology::~Topology() {
	hwloc_topology_destroy(_topology);
}


int Topology::curr_cpu_id() {
	return sched_getcpu();
}

int Topology::max_node_id() const {
	return _nodes.size()-1;
}

int Topology::max_cpu_id() const {
	return _cpu_to_node.size() - 1;
}

const Topology::NumaNode* Topology::get_node(int n) const {
	assert(n >= 0 && n < _nodes.size());
	return _nodes[n];
}

const Topology::NumaNode* Topology::node_of_cpuid(int cpu) const {
	assert (cpu >= 0 && cpu < _cpu_to_node.size());
	return _cpu_to_node[cpu];
}

const Topology::NumaNode* Topology::curr_numa_node() const {
	return node_of_cpuid(curr_cpu_id());
}

int Topology::cores_on_node(int n) const {	// how many cores on this node?
	return get_node(n)->cpus.size();
}

int Topology::core_of_cpuid(int cpu) const {	// return on-chip core no.
	return node_of_cpuid(cpu)->core_of(cpu);
}

void Topology::print(std::ostream &stream) const {
	for (size_t i = 0; i < _nodes.size(); i++) {
		if (_nodes[i] == nullptr) continue;
		
		stream << "Node [" << i << "]\n";
		stream << "\tCpus: [ ";
		for (auto cpu : _nodes[i]->cpus) stream << cpu << " ";
		stream << "]\n";
		stream << "\tDistances: [ ";
		for (auto dist : _nodes[i]->distances) stream << dist << " ";
		stream << "]\n";
	}
}

}
}
