#pragma once


#include <vector>
#include <iostream>
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
		
		int core_of(int cpuid) const;
	};

private:	
	hwloc_topology_t            _topology;
	std::vector<NumaNode*>      _nodes;
	std::vector<NumaNode*>      _cpu_to_node;
	
	Topology();
	~Topology();

public:
	
	static const Topology* get();
	
	static int curr_cpu_id();
	
	int max_node_id() const;
	int max_cpu_id() const;
	
	const NumaNode* get_node(int n) const;
	const NumaNode* node_of_cpuid(int cpu) const;
	const NumaNode* curr_numa_node() const;
	
	int cores_on_node(int n) const;		// how many cores on this node?
	int core_of_cpuid(int cpu) const;	// return on-chip core no.
	
	void print(std::ostream &stream) const;
};



}
}
