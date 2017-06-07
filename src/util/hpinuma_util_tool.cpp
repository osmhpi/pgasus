
#include <iostream>

#include "topology.hpp"
#include "base/node.hpp"

int main()
{
    std::cout << "## PGASUS/hpinuma_util" << std::endl << std::endl;
    std::cout << "# Detected NUMA topology:" << std::endl;
    numa::util::Topology::get()->print(std::cout);
    std::cout << std::endl;

    std::cout << "# Configured NUMA nodes:" << std::endl;
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

    return 0;
}
