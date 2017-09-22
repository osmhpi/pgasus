#pragma once

#include <cassert>
#include <cstdio>

#include <functional>
#include <vector>

#include "PGASUS/base/node.hpp"
#include "timer.hpp"

// must be specialized for whatever classes
//template <class T>
//struct LocalityController {
//	static inline 
//	std::enable_if<is_pod<T>::value, bool>::type
//	isOnNode(const T* val, int node) {
//		return getNumaNodeForMemory((void*) val) == node;
//	}
//};
template <class T>
struct IsOnNode {
	static inline typename std::enable_if<std::is_pod<T>::value, bool>::type
	get(NodeLocalityChecker &checker, const T* val, int node) {
		return checker.get((void*) val) == node;
	//	return getNumaNodeForMemory((void*) val) == node;
	}
};


enum MigrationPolicy { PULL, PUSH };


template <class Executor, class T>
class AbstractExecutor {
public:
	typedef std::function<T()> Generator;
	
private:
	numa::Node _from, _to;
	int _elements;
	bool _check;
	Generator _generator;
	
	/**
	 Interface for implementing CRTP-subclasses:
	 	Collection* generate();
	 	[Iterable over T*] items(Collection*)
	 	void migrate(Collection*);
	 	int migrationCpu();
	*/

public:
	AbstractExecutor() {
		_check = false;
		_elements = -1;
	}
	
	void setFromTo(numa::Node f, numa::Node t) {
		_from = f;
		_to = t;
	}
	
	void setElements(int e, const Generator &gen) {
		_elements = e;
		_generator = gen;
	}
	
	void setCheckLocation(bool b) {
		 _check = b;
	}
	
	inline T construct() {
		return _generator();
	}
	
	inline numa::Node from() const { return _from; }
	inline numa::Node to() const { return _to; }
	inline int elements() const { return _elements; }
	
	int run(int runCount) {
		assert(_from.valid() && _to.valid());
		
		std::vector<typename Executor::Collection*> cols;
		cols.reserve(runCount);
		
		Executor *exec = static_cast<Executor*>(this);
		int t = 0;
		
		// generate
		runAt(_from, [=,&cols]() {
			for (int i = 0; i < runCount; i++) {
				cols.push_back(exec->generate());
			}
		});
		
		// check source object location
		if (_check) {
			NodeLocalityChecker checker;
			size_t wrong = 0, total = 0;
			for (auto col : cols) {
				const auto &items = exec->items(col);
				total += items.size();
				for (const T* val : items) {
					if (!IsOnNode<T>::get(checker, val, _from.physicalId())) wrong++;
				}
			}
			if (wrong > 0) printf(
				"Fail: %zu/%zu objs not on node %d after migration\n",
				wrong, total, _from.physicalId());
		}
		
		// migrate
		runAt(exec->migrationNode(), [=,&t,&cols]() {
			Timer<int> timer(true);
			for (int i = 0; i < runCount; i++) {
				exec->migrate(cols[i]);
			}
			t = timer.stop_get();
		});
		
		// check dest object location
		if (_check) {
			NodeLocalityChecker checker;
			size_t wrong = 0, total = 0;
			for (auto col : cols) {
				const auto &items = exec->items(col);
				total += items.size();
				for (const T* val : items) {
					if (!IsOnNode<T>::get(checker, val, _to.physicalId())) wrong++;
				}
			}
			if (wrong > 0) printf(
				"Fail: %zu/%zu objs not on node %d after migration\n",
				wrong, total, _to.physicalId());
		}
		
		// cleanup
		for (auto col : cols) delete col;
		cols.clear();
		
		return t;
	}
};



