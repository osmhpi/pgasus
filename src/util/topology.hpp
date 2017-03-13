#pragma once


#include <vector>
#include <iostream>
#include <hwloc.h>


namespace numa {
namespace util {


class Topology {
public:
	struct NumaNode {
		size_t                     id;
		std::vector<size_t>        cpus;
		std::vector<size_t>        distances;	// to other NUMA nodes
		
		size_t core_of(size_t cpuid) const;
	};

private:	
	hwloc_topology_t            _topology;
	std::vector<NumaNode*>      _nodes;
	std::vector<NumaNode*>      _cpu_to_node;
	
	Topology();
	~Topology();

public:
	
	static const Topology* get();
	
	static size_t curr_cpu_id();
	
	size_t max_node_id() const;
	size_t max_cpu_id() const;
	
	const NumaNode* get_node(size_t n) const;
	const NumaNode* node_of_cpuid(size_t cpu) const;
	const NumaNode* curr_numa_node() const;
	
	size_t cores_on_node(size_t n) const;		// how many cores on this node?
	size_t core_of_cpuid(size_t cpu) const;	// return on-chip core no.
	
	void print(std::ostream &stream) const;
};



}
}
