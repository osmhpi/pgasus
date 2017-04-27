#include <cstdlib>
#include <cstdio>

#include <map>
#include <unordered_map>
#include <mutex>
#include <functional>

#include "test.hpp"

#include <libcuckoo/cuckoohash_map.hh>
#include <tbb44/tbb/concurrent_hash_map.h>
#include <tbb44/tbb/parallel_for.h>
#include <tbb44/tbb/parallel_reduce.h>
#include "hashtable/hashtable.hpp"

//
// Key generators
//
template <> struct Generator<std::string> {
	static inline std::string generate(size_t idx) {
		char buff[256];
		sprintf(buff, "string%zdfoobar", idx ^ 0xDEADBEEF);
		return std::string(buff);
	}
};

template <> struct Generator<size_t> {
	static inline size_t generate(size_t idx) { return idx ^ 0xDEADBEEF; }
};

template <class T> struct ComputeSum { static inline size_t sum(T t) { return t; } };
template <> struct ComputeSum<std::string> { static inline size_t sum(const std::string &s) { return s.length(); } };


//
// Map traits: std::unordered_map / std::map
//
template <class _MapType, class _KeyType>
struct StdMapBench : public MapBenchmarker<StdMapBench<_MapType, _KeyType>>
{
	typedef _MapType MapType;
	typedef _KeyType KeyType;

	MapType map;
	std::mutex mutex;
	const char *n;

	StdMapBench(const char *nn) : n(nn) {}
	inline const char *name() const { return n; }
	size_t count() { return map.size(); }

	inline void insert(const KeyType &k, size_t val) {
		std::lock_guard<std::mutex> guard(mutex);
		map[k] = val;
	}

	inline size_t lookup(const KeyType &k) {
		std::lock_guard<std::mutex> guard(mutex);
		return map[k];
	}

	inline void remove(const KeyType &k) {
		std::lock_guard<std::mutex> guard(mutex);
		map.erase(k);
	}

	inline size_t size() {
		std::lock_guard<std::mutex> guard(mutex);
		return map.size();
	}

	inline size_t sum() {
		size_t s = 0;
		numa::wait(numa::async<void>([&]() {
			for (auto it = map.begin(); it != map.end(); ++it) s += ComputeSum<KeyType>::sum(it->first) + it->second;
		}, 0));
		return s;
	}
};


//
// Map traits: libcuckoo
//
template <class _KeyType>
struct LibCuckooBench : public MapBenchmarker<LibCuckooBench<_KeyType>>
{
	typedef _KeyType KeyType;
	typedef cuckoohash_map<KeyType, size_t, std::hash<KeyType>> MapType;

	MapType map;

	inline const char *name() const { return "libcuckoo"; }
	size_t count() { return map.size(); }

	inline void insert(const KeyType &k, size_t val) {
		map.insert(k, val);
	}

	inline size_t lookup(const KeyType &k) {
		return map.find(k);
	}

	inline size_t size() {
		return map.size();
	}

	inline void remove(const KeyType &k) {
		map.erase(k);
	}

	inline size_t sum() {
		size_t s = 0;
		numa::wait(numa::async<void>([&]() {
			auto locked = map.lock_table();
			for (auto it = locked.cbegin(); it != locked.cend(); ++it) {
				s += ComputeSum<KeyType>::sum(it->first) + it->second;
			}
		}, 0));
		return s;
	}
};


//
// Map traits: numa::HashTable
//
template <class _KeyType>
struct NumaHashTableBench : public MapBenchmarker<NumaHashTableBench<_KeyType>>
{
	typedef _KeyType KeyType;
	typedef numa::HashTable<KeyType, size_t, 5> MapType;

	MapType map;

	NumaHashTableBench() : map(numa::NodeList::allNodes()) {}

	inline const char *name() const { return "numa::HashTable"; }
	size_t count() { return map.size(); }

	inline void insert(const KeyType &k, size_t val) {
		map[k] = val;
	}

	inline size_t lookup(const KeyType &k) {
		return map[k];
	}

	inline void remove(const KeyType &k) {
		map.remove(k);
	}

	inline size_t size() {
		return map.size();
	}

	inline size_t sum() {
		auto thdatas = numa::containers::for_each_distr<size_t>(map,
			[](const std::pair<const KeyType, size_t> &val, size_t &thdata) {
				thdata += ComputeSum<KeyType>::sum(val.first) + val.second;
		}, 0);
		size_t ret = 0;
		for (auto th : thdatas) ret += *th;
		return ret;
	}
};


//
// Map traits: Intel TBB concurrent_hash_map
//
template <class _KeyType>
struct TbbCHMBench : public MapBenchmarker<TbbCHMBench<_KeyType>>
{
	typedef _KeyType KeyType;
	typedef tbb::concurrent_hash_map<KeyType, size_t> MapType;

	MapType map;

	TbbCHMBench() {
	}

	inline const char *name() const { return "tbb::concurrent_hash_map"; }
	size_t count() { return map.size(); }

	inline void insert(const KeyType &k, size_t val) {
		map.insert(typename MapType::value_type(k, val));
	}

	inline size_t lookup(const KeyType &k) {
		typename MapType::const_accessor ret;
		map.find(ret, k);
		return ret->second;
	}

	inline void remove(const KeyType &k) {
		map.erase(k);
	}

	inline size_t size() {
		return map.size();
	}

	inline size_t sum() {
		std::atomic<size_t> s(0);
		tbb::parallel_for(map.range(), [&](const typename MapType::range_type &range) {
			size_t sub = 0;
			for (auto it = range.begin(); it != range.end(); ++it)
				sub += ComputeSum<KeyType>::sum(it->first) + it->second;
			s += sub;
		});
		return s.load();
	}
};


int main (int argc, char const* argv[])
{
	if (argc < 3) {
		printf("Usage: %s iterations maptype\n", argv[0]);
		return 1;
	}

	size_t elems;
	if (sscanf(argv[1], "%zu", &elems) != 1) {
		return 1;
	}

	std::string maptype = argv[2];

	if (maptype == "std") {
		StdMapBench<std::unordered_map<std::string, size_t>, std::string> bench2("std::unordered_map");
		bench2.run(elems, 1.0f);
	}
	else if (maptype == "cuckoo") {
		LibCuckooBench<std::string> bench3;
		bench3.run(elems, 1.0f);
	}
	else if (maptype == "numa") {
		NumaHashTableBench<std::string> bench4;
		bench4.run(elems, 1.0f);
	}
	else if (maptype == "tbb") {
		TbbCHMBench<std::string> bench5;
		bench5.run(elems, 1.0f);
	}
	else {
		printf("map type not in [std, cuckoo, numa, tbb]\n");
		return 1;
	}

	return 0;
}

