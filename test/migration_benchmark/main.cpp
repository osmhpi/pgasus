/*

Test the performance of object migrations.

For a templated object type T, a configurable number of items, measure time to
migrate the data from node A to node B. also measure, if that was successful (by
checking move_pages)(). measure with the data all in the cache vs. not in cache

parameters:
- template class T
- object count / total memory usage
- is-in-cpu-cache?

functionality:
- measure time
- measure success via move_pages()

different methods:
- for POD: move_pages:
	pull/push
- object copy constructor (of course, for our argumentation, this is not reliable)
	pull
- migrate msource
	pull/push

for the 2nd: test different malloc implementations?


provide general framework:
	- run threads/tasks on node A, B.
	- need to do without libnuma++, as we need different malloc impls.
	- step1: generate. may need to spawn threads somewhere (should, to avoid caching)
	- step2: clear cache?
	- step3: spawn thread at dst node, perform migration

steps 1+3 need to be provided by the following migration methods:
	- PODs move_pages pull/push
	- object-copy constructor
	- msource push/pull

run_test<T,Executor> (count, cached)

used as in:

class POD_move_pages_executor {
	bool pull/push
	
	constructor(from, to, count)
	
	// where to spawn theads?
	int generationNode()
	int migrationNode()
	
	// assured to run on specified node
	void generate();
	void migrate()
}

int t = run_test<SomePod, POD_move_pages_executor<SomePod>>(1024, true);


*/

#include "nodehelper.h"
#include "executor.hpp"
#include "executor_movepages.hpp"
#include "executor_copy.hpp"
#include "executor_msource.hpp"

#include "test_helper.h"


struct TestPod {
	float f[16];
	int i[16];
	char c[128];
};


template <class T>
struct TypeName {
	static inline std::string get() {
		return typeid(T).name();
	}
};


template <class T>
struct TestRunner {
	numa::Node from;
	numa::Node to;
	int elements;
	int runCount;
	std::function<T()> generator;
	
	TestRunner(numa::Node _from, numa::Node _to, int _elems, int _runCount, const std::function<T()> &_gen)
		: from{ _from }
		, to{ _to }
		, elements{ _elems }
		, runCount{ _runCount }
		, generator{ _gen }
	{
	}
	
	template <class Executor>
	void run(Executor exec, const std::string &descr) {
		exec.setFromTo(from, to);
		exec.setElements(elements, generator);
		exec.setCheckLocation(true);
		int t = exec.run(runCount);
		
		printf("[%s] %d: %s (%d runs, %d objs each)\n", 
			TypeName<T>::get().c_str(), t, descr.c_str(), runCount, elements);
	}
};


//
// std::string
//
template <> struct TypeName<std::string> {
	static inline std::string get() { return "std::string"; }
};
template <> struct IsOnNode<std::string> {
	static inline bool get(NodeLocalityChecker &checker, const std::string* val, int node) {
		if (getNumaNodeForMemory((void*) val) != node) return false;
		if (val->size() > 0 && getNumaNodeForMemory((void*) &((*val)[0])) != node) return false;
		return true;
	}
};


//
// std::vector
//
typedef std::vector<int> IntVec;
template <> struct TypeName<IntVec> {
	static inline std::string get() { return "vector<int>"; }
};
template <> struct IsOnNode<IntVec> {
	static inline bool get(NodeLocalityChecker &checker, const IntVec* val, int node) {
		return (getNumaNodeForMemory((void*) val) == node
				&& getNumaNodeForMemory((void*) &((*val)[0])) == node);
	}
};


//
// MyComplexType
//
struct MyComplexType {
	std::vector<int> vec;
	MyComplexType() {
		unsigned r = rand();
		size_t sz = 16 + r % 256;
		vec.reserve(sz);
		for (size_t i = 0; i < sz; i++) vec.push_back(rand());
	}
	MyComplexType(const MyComplexType& other) { *this = other; }
	MyComplexType(MyComplexType &&other) { *this = other; }
	
	MyComplexType& operator=(const MyComplexType &other) {
		vec.reserve(other.vec.size());
		for (auto it : other.vec) vec.push_back(it);
		return *this;
	}
	MyComplexType& operator=(MyComplexType &&) = delete;
};
template <> struct IsOnNode<MyComplexType> {
	static inline bool get(NodeLocalityChecker &checker, const MyComplexType* val, int node) {
		return (getNumaNodeForMemory((void*) val) == node
				&& getNumaNodeForMemory((void*) &(val->vec[0])) == node);
	}
};


//
// std::map<std::string, int>
//
typedef std::map<int, int> IntMap;
template <> struct TypeName<IntMap> {
	static inline std::string get() {
		return "map<int,int>";
	}
};
template <> struct IsOnNode<IntMap> {
	static inline bool get(NodeLocalityChecker &checker, const IntMap* val, int node) {
		if (getNumaNodeForMemory((void*) val) != node)
			return false;
		
		for (auto &it : *val) {
			if (getNumaNodeForMemory((void*) &it.first) != node
				|| getNumaNodeForMemory((void*) &it.second) != node)
				return false;
		}
		
		return true;
	}
};


int main (int argc, char const* argv[])
{
	testing::initialize();

	const numa::NodeList &allNodes = numa::NodeList::logicalNodes();
	numa::NodeList nodes;
	for (const numa::Node &node : allNodes) {
		if (node.memorySize() == 0 || node.cpuCount() == 0) {
			continue;
		}
		nodes.push_back(node);
	}

	numa::Node from = nodes.front();
	numa::Node to = nodes.back();

	TestRunner<TestPod> podTest(from, to, 10000, 100, []() { return TestPod(); });
	podTest.run(MovePagesExecutor<TestPod>(MigrationPolicy::PULL), "MovePages(Pull)");
	podTest.run(MovePagesExecutor<TestPod>(MigrationPolicy::PUSH), "MovePages(Push)");
	podTest.run(CopyExecutor<TestPod>(), "Copy()");
	podTest.run(MsourceExecutor<TestPod>(), "MSource()");
	
	
	TestRunner<std::string> strTest(from, to, 10000, 100, []() {
		unsigned r = rand();
		size_t size = 16 + r % 256;
		return std::string(size, 'A');
	});
	strTest.run(CopyExecutor<std::string>(), "Copy()");
	strTest.run(MsourceExecutor<std::string>(), "MSource()");
	
	
	TestRunner<IntVec> vecTest(from, to, 10000, 100, []() {
		unsigned r = rand();
		size_t size = 16 + r % 256;
		return IntVec(size, rand());
	});
	vecTest.run(CopyExecutor<IntVec>(), "Copy()");
	vecTest.run(MsourceExecutor<IntVec>(), "MSource()");
	
	
	TestRunner<MyComplexType> myTypeTest(from, to, 10000, 100, []() { return MyComplexType(); });
	myTypeTest.run(CopyExecutor<MyComplexType>(), "Copy()");
	myTypeTest.run(MsourceExecutor<MyComplexType>(), "MSource()");
	
	
	TestRunner<IntMap> mapTest(from, to, 100, 100, []() {
		IntMap map;
		for (int i = 0; i < 100; i++) {
			map[i] = i;
		}
		return map;
	});
	mapTest.run(CopyExecutor<IntMap>(), "Copy()");
	mapTest.run(MsourceExecutor<IntMap>(), "MSource()");
	
	return 0;
}
