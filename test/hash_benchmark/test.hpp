#include <cassert>
#include <string>
#include <atomic>

#include "rand.hpp"
#include "timer.hpp"

#include "PGASUS/tasking/tasking.hpp"

/*

For a given concurrent hash map implementation, run a number of benchmarks, 
configurable by:
	- insert: number of elements
	- hit probability (lookup)
	- iteration
	- mixed workloads?

these benchmarks should show system-wide scalability, compared to non-NUMA 
concurrent hash maps.

NUMA optimizations specifically for my version:
	- group local inserts


that should result in the following metrics:
- inserts/sec
- lookups/sec            // depending on hit probability?)
- iterations/sec         // how many items were accessed
- deletes/sec

hash tables should be tested for
- HashTable<std::string, size_t>
- HashTable<size_t, size_t>

*/

template <class KeyType>
struct Generator {
	inline KeyType generate(size_t idx) { assert(false); return KeyType(); }
};

template <class CRTP>
struct MapBenchmarker
{
	void run(size_t elems, float hit) {
		Timer<int> timer(true);

		CRTP *crtp = static_cast<CRTP*>(this);
		std::atomic_size_t *id = new std::atomic_size_t(0);


		std::atomic_size_t *counter = new std::atomic_size_t(0);
		size_t step = 5000;

		numa::wait(numa::forEachThread(numa::NodeList::logicalNodes(), [=]() {
			id->fetch_add(1);
			Generator<typename CRTP::KeyType> gen;

			while (true) {
				size_t nextBunch = counter->fetch_add(step);
				if (nextBunch >= elems) return;

				for (size_t i = nextBunch; i < nextBunch + step; i++) {
					crtp->insert(gen.generate(i), i);
				}
			}
		}, 0));
		int tInsert = timer.stop_get_start();


		numa::wait(numa::forEachThread(numa::NodeList::logicalNodes(), [=]() {
			size_t totalLookups = elems;
			size_t myLookups = totalLookups / id->load();
			size_t maxId = elems / hit;
			size_t result = 0;
			Generator<typename CRTP::KeyType> gen;
			FastRand<size_t> rand;

			for (size_t i = 0; i < myLookups; i++) {
				typename CRTP::KeyType k = gen.generate(rand() % maxId);
				result += crtp->lookup(k);
			}

			return result;
		}, 0));
		int tLookup = timer.stop_get_start();

		size_t sz1 = crtp->size();
		size_t sum = crtp->sum();
		int tIterate = timer.stop_get_start();

		counter->store(0);

		numa::wait(numa::forEachThread(numa::NodeList::logicalNodes(), [=]() {
			Generator<typename CRTP::KeyType> gen;

			while (true) {
				size_t nextBunch = counter->fetch_add(step);
				if (nextBunch >= elems) return;

				for (size_t i = nextBunch; i < nextBunch + step; i++) {
					crtp->remove(gen.generate(i));
				}
			}
		}, 0));
		int tRemove = timer.stop_get_start();

		size_t sz2 = crtp->size();

		printf("elems=[%zu,%zu], sum=%zu, insert=%d, lookup=%d, iterate=%d, remove=%d\t\t%s\n",
			   sz2, sz1, sum, tInsert, tLookup, tIterate, tRemove, crtp->name());

		delete counter;
		delete id;
	}
};
