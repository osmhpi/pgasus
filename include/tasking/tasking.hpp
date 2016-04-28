#pragma once

#include <list>

#include "base/node.hpp"
#include "tasking/synchronizable.hpp"
#include "tasking/task.hpp"


namespace numa {


namespace tasking {
class Task;
void spawn_task(const Node &node, Task *task);
}

void wait(const std::list<TriggerableRef> &tasks);
void wait(const TriggerableRef &ref);
void yield();

/**
 * Makes sure the worker thread's thread-local msources
 * are populated with enough pages
 */
void prefaultWorkerThreadStorages(size_t bytes);

/**
 * Waits for task completion and returns result
 */
template <class T>
T get_result(const TaskRef<T> &ref) {
	std::list<TriggerableRef> list;
	list.push_back(ref);
	wait(list);
	return ref->get();
}

/**
 * Spawns a task that executes the specified function, return a Future<T> to
 * that task. This reference can be waited upon and the result value retrieved.
 */
template <class T>
TaskRef<T> async(const numa::tasking::TaskFunction<T> &fun, Priority prio, const Node &node = Node()) {
	if (node.valid()) numa::malloc::push(numa::Place(node));
	TaskRef<T> task = tasking::FunctionTask<T>::create(fun, prio);
	if (node.valid()) numa::malloc::pop();

	tasking::spawn_task(node, task.get());
	return task;
}

/**
 * Spawns the given task on each worker thread's task queue on all
 * the given nodes
 */
std::list<TriggerableRef> forEachThread(const NodeList &nodes, const numa::tasking::TaskFunction<void> &fun, Priority prio);

template <class T>
struct DistributedExec {
	typedef std::vector<T> ResultType;

	ResultType result;

	DistributedExec(const tasking::TaskFunction<T> &fun, Priority prio, const NodeList &nodes) {
		std::vector<TaskRef<T> > tasks;
		std::list<TriggerableRef> refs;

		for (const Node& node : nodes) {
			int cpus = node.cpuCount();
			for (int i = 0; i < cpus; i++) {
				tasks.push_back(spawn(fun, prio, node));
				refs.push_back(TriggerableRef(tasks.back().get()));
			}
		}

		wait(refs);
		for (auto &task : tasks)
			result.push_back(task->get());
	}
};

template <>
struct DistributedExec<void> {
	typedef int ResultType;
	ResultType result = 0;

	DistributedExec(const tasking::TaskFunction<void> &fun, Priority prio, const NodeList &nodes) {
		std::list<TriggerableRef> refs;

		for (const Node& node : nodes) {
			int cpus = node.cpuCount();
			for (int i = 0; i < cpus; i++)
				refs.push_back(async(fun, prio, node));
		}

		wait(refs);
	}
};

template <class T>
auto distributedExec(const tasking::TaskFunction<T> &fun, Priority prio, const NodeList &nodes)
	-> typename DistributedExec<T>::ResultType {
	return DistributedExec<T>(fun, prio, nodes).result;
}

}
