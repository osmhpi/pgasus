#include "hashtable/hashtable.hpp"

#include <vector>
#include <string>

#include <cstdio>
#include <cstring>

std::string generate(int i) {
	char buff[4096];
	sprintf(buff, "_%d_", i);
	return buff;
}


int main (int argc, char const* argv[])
{
	numa::HashTable<std::string, int, 5> table(numa::NodeList::allNodes());
	
	int count = 100000;
	
	// fill
	for (int i = 0; i < count; i++) {
		table[generate(i)] = i;

		assert(!table.begin().is_end());
		assert(table.end().is_end());
	}
	
	// check for content
	for (int i = 0; i < count; i++) {
		if (table[generate(i)] != i) {
			printf("Not in table: (%s,%d)\n", generate(i).c_str(), i);
		}
	}

	std::vector<bool> values1(count, false);
	std::vector<bool> values2(count, false);

	// iterate 1...
	for (auto it = table.begin(); !it.is_end(); it.next()) {
		int n;
		assert(sscanf(it->first.c_str(), "_%d_", &n) > 0);
		assert(n == it->second);
		assert(n >= 0 && n < count);
		assert(!values1[n]);
		values1[n] = true;
	}

	// iterate 2...
	for (numa::Node node : table.nodes()) {
		numa::HashTable<std::string, int, 5>::ParallelIteration iter(table, node, 10);
		numa::HashTable<std::string, int, 5>::ParallelIteration::Iterator it;

		numa::MemSource ms;

		while (iter.get(it, ms)) {
			numa::PlaceGuard guard(ms);
			for (; !it.is_end(); it.next()) {
				int n;
				assert(sscanf(it->first.c_str(), "_%d_", &n) > 0);
				assert(n == it->second);
				assert(n >= 0 && n < count);
				assert(!values2[n]);
				values2[n] = true;
			}
		}
	}

	// check for values
	for (int i = 0; i < count; i++) {
		assert(values1[i]);
		assert(values2[i]);
	}
	
	return 0;
}
