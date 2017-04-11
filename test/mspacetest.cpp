#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstdlib>
#include <string>
#include <vector>

#include "base/node.hpp"
#include "msource/msource.hpp"


typedef std::vector<void*> Memories;
typedef std::vector<int> MemSizes;
using numa::MemSource;


int sum(const MemSizes &mems) {
	int s = 0;
	for (size_t i = 0; i < mems.size(); i++) s += mems[i];
	return s;
}


Memories fill(MemSource src, const MemSizes &mems) {
	Memories allocs;
	
	for (size_t i = 0; i < mems.size(); i++) {
		allocs.push_back(src.alloc(mems[i]));
	}
	
	printf("Filled [%s] with %d total alloc bytes\n", src.getDescription().c_str(), sum(mems));
	
	return allocs;
}


void printInfo(MemSource src) {
	numa::msource_info info = src.stats();
	
	printf("Space [%s]: %zd arenas (%zd alloc, %zd used), %zd mmaps (%zd alloc, %zd used)\n",
		src.getDescription().c_str(),
		info.arena_count, info.arena_size, info.arena_used,
		info.hugeobj_count, info.hugeobj_size, info.hugeobj_used);
}



int main (int argc, char const* argv[])
{
	srand(time(0));
	
	MemSizes sizes;
	for (int n = 0; n < 1024; n++) {
		sizes.push_back(rand() % (16 << 10));
	}
	
	MemSource msrc = MemSource::create(numa::Node::curr().physicalId(), 1 << 20, "test");
	
	Memories mems = fill(msrc, sizes);
	printInfo(msrc);
	
	for (size_t i = 0; i < mems.size()/2; i++)
		MemSource::free(mems[i]);
		
	printInfo(msrc);
	fill(msrc, sizes);
	fill(msrc, sizes);
	fill(msrc, sizes);
	printInfo(msrc);
	
	return 0;
}
