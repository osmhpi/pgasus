#pragma once


#include <iostream>
#include <utility>
#include <vector>
#include <hwloc.h>
#include "util/hpinuma_util_export.h"


namespace numa {
namespace util {


class HPINUMA_UTIL_EXPORT Topology {
public:
	struct NumaNode {
		int                     id;
		std::vector<int>        cpus;
		std::vector<int>        distances;	// to other NUMA nodes
		/** Distances to other NUMA nodes sorted with nearest first.
		  * For same distances, the neighbor with smaller ID is listed first. */
		std::vector<std::pair<int, NumaNode*>> nearestNeighbors;
		size_t memorySize;
		
		int core_of(int cpuid) const;
	};

private:	
	hwloc_topology_t            _topology;
	/**
	 * NumaNode structs indexed by their system NUMA node id.
	 * There can be gaps in this list, if the system does not use a continuous
	 * list of node ids.
	 */
	std::vector<NumaNode*>      _nodes;
	std::vector<int>			_nodeIds;
	std::vector<NumaNode*>      _cpu_to_node;
	size_t						_total_cpu_count;
	
	Topology();
	~Topology();

public:
	
	static const Topology* get();
	
	static int curr_cpu_id();
	
	/**
	 * Vector of NUMA node ids on the system.
	 * In the simplest case this is the consecutive list 0..(maxId-1). However,
	 * on some systems there are gaps in the assigned node ids.
	 * It is guaranteed to be in ascending order.
	 */
	const std::vector<int> & node_ids() const;
	size_t number_of_nodes() const;
	size_t total_cpu_count() const;
	const NumaNode* get_node(int n) const;
	const NumaNode* node_of_cpuid(int cpu) const;
	const NumaNode* curr_numa_node() const;
	
	/** @return Number of cores on a NUMA node. Returns -1 if the node id is not
	 * used. */
	int cores_on_node(int n) const;
	int core_of_cpuid(int cpu) const;	// return on-chip core no.
	
	void print(std::ostream &stream) const;
};



}
}
