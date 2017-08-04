
#include <algorithm>
#include <iostream>
#include <string>

#include "PGASUS/base/node.hpp"
#include "PGASUS/base/topology.hpp"
#include "strutil.hpp"

int main()
{
	std::cout << "## PGASUS/hpinuma_util" << std::endl << std::endl;
	std::cout << "# Detected NUMA topology:" << std::endl;
	numa::util::Topology::get()->print(std::cout);
	std::cout << std::endl << std::endl;

	std::cout << "# Configured (logical) NUMA nodes:" << std::endl;
	auto && nodes = numa::NodeList::logicalNodes();
	std::cout << "Output pattern: \"(logical ID) [physical ID] CPUs: X, Threads: Y\"" << std::endl;
	std::cout << "\tCPU IDs: ..." << std::endl << std::endl;

	for (auto && node : nodes) {
		std::cout << '(' << node.logicalId() << ") [" << node.physicalId()
			<< "] CPUs: " << node.cpuCount() << ", Threads: "
			<< node.threadCount() << std::endl;
		std::cout << "\tCPU IDs: ";
		for (const auto id : node.cpuids()) {
			std::cout << id << " ";
		}
		std::cout << std::endl;
		if (!node.valid()) {
			std::cout << "\tNode is invalid!" << std::endl;
		}
	}

	std::cout << std::endl;
	std::cout << "String for OMP_PLACES environment variable (see OpenMP 4.5 Spec. Section 4.5):" << std::endl;
	const std::string ompPlaces = "OMP_PLACES=" + numa::util::concatGenerate(
		nodes.begin(), nodes.end(), ",",
		[] (const decltype(nodes.begin())& it) {
			const numa::Node &node = *it;
			const auto & cpuids = node.cpuids();
			std::string placeStr = "{";
			const bool isConsecutive = cpuids.end() == std::adjacent_find(
				cpuids.begin(), cpuids.end(), [] (int id1, int id2) {
				return id1 + 1 != id2; });
			if (isConsecutive) {
				placeStr += std::to_string(cpuids.front()) + ":"
					+ std::to_string(cpuids.size());
			}
			else {
				placeStr += numa::util::concat(cpuids.begin(), cpuids.end(), ",");
			}
			placeStr += "}";
			return placeStr;
	});
	std::cout << ompPlaces << std::endl;

	return 0;
}
