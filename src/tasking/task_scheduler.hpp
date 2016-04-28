#pragma once

#include <vector>
#include <mutex>
#include <semaphore.h>

#include "msource/msource.hpp"
#include "base/spinlock.hpp"

#include "tasking/task.hpp"
#include "tasking/task_collection.hpp"
#include "tasking/context.hpp"


namespace numa {
namespace tasking {


class WorkerThread;
class ThreadManager;


/**
 * Encapsulates all priorities within one scheduling domain
 */
class SchedulingDomain
{
private:
	typedef numa::SpinLock Lock;
	
	private:
	/*
	 * For each priority level we keep one TaskList (lazily initialized) within
	 * a vector. All active (= containing more than zero tasks) task lists
	 * are additionaly stored in a linked list 
	 */
	struct PriorityTasks
	{
		typedef std::atomic<TaskCollection*> TaskCollectionPtr;
		TaskCollectionPtr       tasks;
		std::atomic<size_t>     count;
		Lock                    mutex;
	
		PriorityTasks() : tasks(nullptr), count(0) {}
		PriorityTasks(const PriorityTasks &o) = delete;
		PriorityTasks(PriorityTasks &&o) = delete;
	};
	
	template <class T> using msvector = numa::msvector<T>;
	template <class T> using mslist   = numa::mslist<T>;

	numa::MemSource             _msource;
	
	Lock                        _active_thread_ids_mutex;	// mutex for access
	mslist<int>                 _active_thread_ids;
	
	std::atomic<size_t>         _topPriorityIdx;			// currently
	msvector<PriorityTasks>     _priorities;				// all priorities

public:

	SchedulingDomain(const numa::MemSource &ms);
	~SchedulingDomain();
	
	/**
	 * Returns a task ready for execution. If there is no such task, return null.
	 * Always picks the highest-priority task. Prefers tasks bound to that thid
	 */
	Task* try_get_task(int thid);
	
	/**
	 * Inserts a task into this scheduling domain
	 */
	void put_task(Task *task, int thid);
	
	/** Adds given thread ID to task collections */
	void add_thread(int idx);
	
	/** Removes given thread ID from task collections */
	void remove_thread(int idx);
};


/**
 * Holds all to-be-scheduled tasks for a set of managed worker threads.
 */
class Scheduler
{
private:
	
	template <class T> using msvector = numa::msvector<T>;
	template <class T> using mslist   = numa::mslist<T>;
	typedef numa::SpinLock Lock;
	
	Node                        _node;
	int                         _cores;
	numa::MemSource             _msource;				// local memory allocator
	
	/**
	 * Local+Global tasks
	 */
	SchedulingDomain           *_domain;
	
	/**
	 * Worker threads working on this node scheduler are referenced by their
	 * node-relative core numbers, starting at 0.
	 */
	msvector<WorkerThread*>     _workers;	// core to worker (or null)
	std::recursive_mutex        _workers_lock;
	ThreadManager              *_thread_manager;

	std::atomic<size_t>         _waitingThreadsCount;
	sem_t                       _waitingThreadsSemaphore;
	
	ContextCache                _ctx_cache;

private:
	
	/**
	 * Create a new thread on the given core, if none is yet running there.
	 * Return the new thread.
	 */
	WorkerThread* create_thread(int core);
	
	/**
	 * Remove thread running on given node core. Waits until the thread has
	 * terminated.
	 */
	void stop_wait_thread(int core);

	/**
	 * Wake N threads from their sleep
	 */
	void taskAvailable();
	
	
public:
	Scheduler(const Node &node);
	~Scheduler();
	
	inline ContextCache& context_cache() { return _ctx_cache; }
	inline Node node() const { return _node; }
	
	/**
	 * Get scheduler for given node, or local scheduler if node<0
	 */
	static Scheduler* get_scheduler(const Node& node = Node());
	
	/**
	 * Sets worker thread count 
	 */
	void set_thread_count(int count);
	
	/**
	 * Sets worker thread by core IDs
	 */
	void set_threads(const std::vector<int> &core_ids);
	
	/**
	 * Introduces the given task to scheduling task queues
	 */
	static void spawn_task(Scheduler *sched, Task* task);

	/**
	 * Returns IDs of all workers
	 */
	std::vector<int> worker_ids();
	
	/**
	 * Returns a task ready for execution from local or global scheduling domains
	 */
	Task* try_get_task(int thid);
	
	/**
	 * Adds task to scheduling task queues. If scheduler is NULL, insert into
	 * global task queues. Else into scheduler's queue.
	 */
	void put_task(Task* t, int thid);

	/**
	 * Wait for a while for a task to be available
	 */
	void waitForTask(size_t usec);
};


}
}
