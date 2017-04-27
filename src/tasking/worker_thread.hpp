#pragma once

#include <atomic>

#include "msource/msource_types.hpp"

#include "tasking/task.hpp"
#include "tasking/task_scheduler.hpp"
#include "tasking/thread_manager.hpp"

#include "base/tsc.hpp"


namespace numa {
namespace tasking {

class Task;


/**
 * A WorkerThread represents a running thread that executes Tasks that it 
 * receives from the associated Scheduler
 */
class WorkerThread : public ThreadBase
{
private:
	typedef numa::msvector<Context*> ContextVec;
	
	Scheduler                  *_scheduler;
	size_t                      _thread_id;
	Node                        _node;
	
	/** The Task the thread is currently working on */
	Task                       *_curr_task;
	
	/** Instructions on what to do with task, in case of task dropping */
	std::list<TriggerableRef>   _task_waits;
	
	/** Thread is always in a context. After a jump, this context changes. */
	Context                    *_curr_ctx;
	
	/** Original, native context. We have to jump back there to exit thread */
	boost::context::fcontext_t  _native_context;
	
	/** Collection of neutral, non-task contexts */
	ContextVec                  _ready_contexes;
	
	std::atomic_int             _done;
	
	sem_t                       _sleep;
	
	/**
	 * Collect fine-grained performance data about where time is spent
	 */
	typedef uint_fast64_t Counter;
	Counter                     _curr_time = 0;			// used to measure diffs
	Counter                     _time_getting_task = 0;	// scheduler->get_task()
	Counter                     _time_running = 0;		// actual task execution
	Counter                     _time_sleeping = 0;		// sleep-wait for job (in get_task())
	Counter                     _time_task_yield = 0;	// managing task state
	Counter                     _time_task_wait = 0;	// managing task state
	Counter                     _time_task_sched = 0;	// managing task state
	Counter                     _time_task_done = 0;	// managing task state
	Counter                     _time_unemployment = 0;	// scheduler unemployment calls (in get_task())
	
	inline Counter reset_get_delta() {
		Counter old = _curr_time;
		_curr_time = numa::util::rdtsc();
		return _curr_time - old;
	}
	
public:
	
	WorkerThread(size_t id, Scheduler *sched, const numa::MemSource &ms);
	~WorkerThread() override;

	inline int id() const { return _thread_id; }
	inline Node homeNode() const { return _node; }
	
	inline Scheduler* scheduler() const { return _scheduler; }
	
protected:

	virtual void run() override;

private:

	Task *get_new_task();
	
	static void start_new_context(intptr_t tcb_ptr);
	
	inline Context *get_neutral_context();
	void put_neutral_context(Context *ctx);
	
	/**
	 * Pauses the execution of the given task for yielding/waiting reasons.
	 * Returns where it left off, when the task gets rescheduled
	 */
	static void drop_task(WorkerThread *self);
	
	/**
	 * Sets the TLS to the given thread 
	 */
	static void set_tls(WorkerThread *th);

public:
	
	/**
	 * Returns this worker thread. Returns null, if running thread is not a
	 * registered worker thread
	 */
	static WorkerThread* curr_worker_thread();
	
	/**
	 * Lets the currently running task wait for the given tasks
	 */
	static void curr_task_wait(const std::list<TriggerableRef> &tasks);
	
	/**
	 * Lets the currently running task yield (i.e. give up execution to be 
	 *  scheduled again later). 
	 */
	static void yield();
	
	/**
	 * Gets called when the thread has registered as unemployed at its
	 * local job center
	 */
	void notify();
	
	void shutdown();
};

}
}
