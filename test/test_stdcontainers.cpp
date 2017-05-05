#include "hashtable/containers.hpp"

#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <list>
#include <string>

#include <cmath>
#include <cstdio>
#include <cstring>


float tediousCalc(int count) {
	float val = 0.f;
	for (int i = 0; i < count * 1000; i++)
		val += sin(i) * cos(i);
	return val;
}


int main (int argc, char const* argv[])
{
	size_t count = 100 * 1000;

	std::vector<float> testVec(count);
	numa::containers::std_for_each_distr<size_t>(testVec, [](float &elem, size_t &param) {
		elem = tediousCalc(1);
		param += 1;
		if (param % 100 == 0) {
			// printf("%zd iters\n", param);
			printf(".");
		}
	}, numa::containers::IgnorePlacement(), 50);

	printf(" done!\n");

	return 0;
}
