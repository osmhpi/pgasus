#pragma once

#include <functional>
#include <map>

#include "base/node.hpp"

void runAt(numa::Node node, const std::function<void()> &function);

struct NodeLocalityChecker {
private:
	std::map<uintptr_t, int> pageNodes;
public:
	int get(void *p);
	void clear();
};

