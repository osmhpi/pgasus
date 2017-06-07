#include "hashtable/hashtable.hpp"

#include <vector>
#include <string>

#include <cstdio>
#include <cstring>

#include "test_helper.h"

std::string generate(int i) {
	char buff[4096];
	sprintf(buff, "_%d_", i);
	return buff;
}


int main (int argc, char const* argv[])
{
	testing::initialize();

	numa::HashTable<std::string, int, 5> table(numa::NodeList::logicalNodes());
	
	int count = 100000;
	
	// fill
	for (int i = 0; i < count; i++) {
		table[generate(i)] = i;

		ASSERT_TRUE(!table.begin().is_end());
		ASSERT_TRUE(table.end().is_end());
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
		ASSERT_TRUE(sscanf(it->first.c_str(), "_%d_", &n) > 0);
		ASSERT_TRUE(n == it->second);
		ASSERT_TRUE(n >= 0 && n < count);
		ASSERT_TRUE(!values1[n]);
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
				ASSERT_TRUE(sscanf(it->first.c_str(), "_%d_", &n) > 0);
				ASSERT_TRUE(n == it->second);
				ASSERT_TRUE(n >= 0 && n < count);
				ASSERT_TRUE(!values2[n]);
				values2[n] = true;
			}
		}
	}

	// check for values
	for (int i = 0; i < count; i++) {
		ASSERT_TRUE(values1[i]);
		ASSERT_TRUE(values2[i]);
	}
	
	return 0;
}
