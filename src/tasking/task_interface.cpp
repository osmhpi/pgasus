#include <pthread.h>

#include "tasking/tasking.hpp"
#include "tasking/task_scheduler.hpp"
#include "tasking/worker_thread.hpp"

#include "util/topology.hpp"


/**
 * Wait for Triggerables from within a native thread
 * Thread will go to sleep during wait.
 */
class NativeThreadWait : public numa::Synchronizer
{
private:
	sem_t       _semaphore;
public:
	NativeThreadWait() {
		if (sem_init(&_semaphore, 0, 0) != 0) {
			assert(false);
		}
	}
	
	virtual void notify() override {
		if (sem_post(&_semaphore) != 0) {
			assert(false);
		}
	}
	
	void wait() {
		if (sem_wait(&_semaphore) != 0) {
			assert(false);
		}
	}
};


namespace numa {

void wait(const std::list<TriggerableRef> &tasks) {
	tasking::WorkerThread *this_wt = tasking::WorkerThread::curr_worker_thread();
	
	// in worker thread
	if (this_wt != nullptr) {
		tasking::WorkerThread::curr_task_wait(tasks);
	}
	// we are running in a non-worker thread:
	else if (!tasks.empty()) {
		NativeThreadWait op;
		if (op.synchronize(tasks)) {
			op.wait();
		}
	}
}

void wait(const TriggerableRef &ref) {
	std::list<TriggerableRef> list;
	list.push_back(ref);
	wait(list);
}

void yield() {
	wait(std::list<TriggerableRef>());
}

/**
 * Spawns the given task on each worker thread's task queue on all
 * the given nodes
 */
std::list<TriggerableRef> forEachThread(const NodeList &nodes, const numa::tasking::TaskFunction<void> &fun, Priority prio) {
	std::list<TriggerableRef> waitList;

	// get total thread count
	size_t count = 0;
	for (Node node : NodeList::allNodes())
		count += tasking::Scheduler::get_scheduler(node)->worker_ids().size();

	// spawn one task for each worker thread
	for (Node node : nodes) {
		tasking::Scheduler *sched = tasking::Scheduler::get_scheduler(node);
		for (int thid : sched->worker_ids()) {
			numa::PlaceGuard guard(node);

			TaskRef<void> task = tasking::FunctionTask<void>::create(fun, prio);

			sched->put_task(task.get(), thid);
			waitList.push_back(task);
		}
	}

	return waitList;
}

void prefaultWorkerThreadStorages(size_t bytes) {
	std::list<TriggerableRef> waitList;

	// get total thread count
	size_t count = 0;
	for (Node node : NodeList::allNodes())
		count += tasking::Scheduler::get_scheduler(node)->worker_ids().size();

	// barrier impl.
	sem_t semaphore;
	std::mutex mutex;
	size_t minPrefault = (size_t)-1;
	std::atomic_size_t counter(0);
	if (sem_init(&semaphore, 0, 0) != 0) {
		assert(false);
	}

	// spawn one task for each worker thread
	wait(forEachThread(NodeList::allNodes(), [bytes,count,&counter,&semaphore,&mutex,&minPrefault]() {
		size_t pf = malloc::curr_msource().prefault(bytes);

		// wait until all others have completed. super ugly
		if (counter.fetch_add(1) == count-1) {
			sem_post(&semaphore);
		}
		sem_wait(&semaphore);
		sem_post(&semaphore);

		// update max. prefault
		std::lock_guard<std::mutex> lock(mutex);
		minPrefault = std::min<size_t>(minPrefault, pf);
	}, Priority::min()));
	
	if (minPrefault == bytes)
		numa::debug::log(numa::debug::DEBUG, "Prefaulted %zd bytes on %zd thread msources", bytes, count);
	else
		numa::debug::log(numa::debug::CRITICAL, "Prefaulted %zd bytes (%zd requested) on %zd thread msources", minPrefault, bytes, count);
}

namespace tasking {
void spawn_task(const Node &node, Task *task) {
	Scheduler *sched = node.valid() ? Scheduler::get_scheduler(node) : nullptr;
	Scheduler::spawn_task(sched, task);
}
}

}
