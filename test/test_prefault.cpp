#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>
#include <sstream>

#include <vector>
#include <list>
#include <iostream>

#include "tasking/tasking.hpp"
#include "test_helper.h"
#include "timer.hpp"


using numa::TaskRef;
using numa::TriggerableRef;

void usage(const char *name) {
	printf("Usage: %s bytes\n", name);
	exit(0);
}

int main (int argc, char const* argv[])
{
	testing::initialize();

	if (argc < 2) usage(argv[0]);

	// get prefault byte count
	size_t count;
	if (sscanf(argv[1], "%zu", &count) < 1)
		usage(argv[0]);

	std::list<TriggerableRef> waitList;

	// spawn some long-running tasks to test if every core
	// actually received a prefaulting task
	for (size_t i = 0; i < numa::NodeList::logicalNodesCount() * 2; i++) {
		waitList.push_back(numa::async<float>([]() -> float {
			printf("dummy start\n");
			float ret = 0.f;
			for (int i = 0; i < 7000000; i++)
				ret += sin(i) * cos(i);
			printf("dummy finished\n");
			return ret;
		}, 0));
	}

	printf("prefaulting start\n");
	numa::prefaultWorkerThreadStorages(count);
	printf("prefaulting done\n");

	// getc(stdin);

	return 0;
}
