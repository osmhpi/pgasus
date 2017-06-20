#include "msource/msource.hpp"
#include "msource/msource_types.hpp"
#include "msource/mmaphelper.h"

#include "tasking/tasking.hpp"
#include "base/tsc.hpp"

#include <cstdio>
#include <cstdlib>

inline std::map<unsigned int,unsigned int> *randomMap(size_t size) {
	std::map<unsigned int,unsigned int> *ret = new std::map<unsigned int,unsigned int>();
	while (ret->size() < size)
		(*ret)[(unsigned int)rand()] = (unsigned int)rand();
	return ret;
}

unsigned int accessMap(const std::map<unsigned int,unsigned int> &map) {
	int ret = 0;
	for (auto it = map.begin(); it != map.end(); ++it) {
		ret += it->first + it->second;
	}
	return ret;
}

using numa::util::TscTime;

int main (int argc, char const* argv[])
{
	if (argc < 2) {
		printf("Usage: %s elems\n", argv[0]);
		return 1;
	}

	// read param
	int elems;
	if (sscanf(argv[1], "%d", &elems) != 1) {
		printf("invalid elems %s", argv[1]);
		return 1;
	}

	srand(time(0));

	// go through all node combinations
	numa::NodeList allNodes = numa::NodeList::logicalNodesWithCPUs();
	for (size_t fromNodeIdx = 0; fromNodeIdx < allNodes.size(); fromNodeIdx++) {
		numa::Node fromNode = allNodes[fromNodeIdx];

		for (size_t toNodeIdx = fromNodeIdx; toNodeIdx < allNodes.size(); toNodeIdx++) {
			numa::Node toNode = allNodes[toNodeIdx];
			std::map<unsigned int,unsigned int> *map;
			TscTime timeRemote = 0, timeMigrate = 0, timeLocal = 0;
			size_t nPages = 0;

			//
			// test direct access
			//
			numa::wait(numa::async<void>([&]() {
				map = randomMap(elems);
			}, 0, fromNode));

			numa::wait(numa::async<void>([&]() {
				TscTime t1 = numa::util::rdtsc();
				accessMap(*map);
				TscTime t2 = numa::util::rdtsc();
				timeRemote += t2-t1;
			}, 0, toNode));

			delete map;

			//
			// test migrate + access
			//
			numa::MemSource ms;
			numa::wait(numa::async<void>([&]() {
				ms = numa::MemSource::create(fromNode, 1 << 30, "remote");
				numa::PlaceGuard guard(ms);
				map = randomMap(elems);
			}, 0, fromNode));

			numa::wait(numa::async<void>([&]() {
				TscTime t1 = numa::util::rdtsc();
				nPages += ms.migrate(toNode);
				TscTime t2 = numa::util::rdtsc();
				accessMap(*map);
				TscTime t3 = numa::util::rdtsc();
				timeMigrate += t2-t1;
				timeLocal += t3-t2;
			}, 0, toNode));

			delete map;

			//
			// print result
			//
			printf("src=%d dst=%d remote=%zd migrate=%zd (%zd pages) local=%zd\n",
				   fromNode.physicalId(), toNode.physicalId(), timeRemote/1000, timeMigrate/1000, nPages, timeLocal/1000);
		}
	}

	return 0;
}
