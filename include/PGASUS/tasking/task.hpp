#pragma once

#include <list>
#include <cstdint>

#include "PGASUS/base/spinlock.hpp"
#include "PGASUS/base/ref_ptr.hpp"
#include "PGASUS/hpinuma_export.h"
#include "PGASUS/malloc.hpp"
#include "PGASUS/tasking/synchronizable.hpp"

namespace numa {

struct HPINUMA_EXPORT Priority {
	int_least8_t value;
	
	Priority() : value(min().value) {}
	constexpr Priority(int_least8_t v) : value(v) {}
	
	static Priority constexpr min() { return Priority(-16); }
	static Priority constexpr max() { return Priority(16); }
	
	constexpr size_t index() const {
		return (size_t) ((ssize_t)value - (ssize_t)min().value);
	}
	
	static constexpr size_t max_index() {
		return max().index();
	}
	
	inline bool operator<(const Priority &other) const { return value < other.value; }
	inline bool operator>(const Priority &other) const { return value > other.value; }
};

namespace tasking {

// forward decl.
class Context;
class Scheduler;
class WorkerThread;

/**
 * Task object that is used within tasking subsystem. 
 * It is exported to the user through a RefPtr<Task>.
 * Schedulers and threads call state change methods.
 */
class HPINUMA_EXPORT Task : public TwoPhaseTriggerable, public Synchronizer
{
	friend class Scheduler;
	friend class WorkerThread;
	
protected:
	/**
	 * Task States
	 */
	static constexpr uint16_t READY          = 1;
	static constexpr uint16_t RUNNING        = 2;
	static constexpr uint16_t WAITING        = 3;
	static constexpr uint16_t SUSPENDED      = 4;
	static constexpr uint16_t COMPLETED      = 5;
	
	/**
	 * Flags + Mask for state
	 */
	static constexpr uint16_t KEEP_THREAD    = 0x8000;
	static constexpr uint16_t KEEP_SCHEDULER = 0x4000;
	static constexpr uint16_t HAS_STARTED    = 0x2000;
	static constexpr uint16_t FLAG_MASK      = 0xE000;
	
private:
	typedef numa::SpinLock Lock;
	
	uint16_t                                _state_flags;
	Priority                                _priority;
	
	Scheduler                              *_scheduler;
	WorkerThread                           *_home_thread;
	
	Context                                *_context;
	
	Lock                                    _mutex;
	
	numa::malloc::PlaceStack                _place_stack;

protected:
	virtual void notify() override;
	virtual void do_run() = 0;
	
	/**
	 * Create the task, bind it to the given scheduler
	 */
	explicit Task(Priority prio);

	virtual ~Task();
	
private:
	
	inline void set_state(uint16_t st) {
		uint16_t flags = _state_flags & FLAG_MASK;
		_state_flags = st | flags;
	}
	
	/**
	 * Start or continue execution of the task in the given thread.
	 * This doesn't inform the scheduler as we expect the scheduler to have it
	 * removed already.
	 */
	void schedule(WorkerThread *th);
	
	/**
	 * Wait for other task to complete. Returns true, if a state change to
	 * a waiting state has happened (the other task may have already completed)
	 */
	bool wait(const TriggerableRef &ref);
	
	/** 
	 * Wait for a number of tasks to complete. Returns true, if a state change to
	 * a waiting state has happened (the other tasks may have already completed)
	 */
	bool wait(const std::list<TriggerableRef> &refs);
	
	/**
	 * Suspends execution and bringts it back to the scheduler.
	 */
	void yield(size_t th_idx);
	
	/**
	 * Marks the task as completed. Informs waiting tasks and threads.
	 */
	void done();

	/**
	 * Calls the Task function, assigns the task's context.
	 * Returns the worker thread that has finished the task
	 */
	WorkerThread* run(Context *ctx);
	
	Context* get_context();
	
	size_t home_thread_id() const;
	
public:
	
	Node node() const;
	int cpuid() const;
	
	inline Priority priority() const { 
		return _priority; 
	}
	
	inline uint16_t state() const {
		return _state_flags & ~FLAG_MASK;
	}
	
	inline bool has_started() const { return (_state_flags & HAS_STARTED) != 0; }
	inline bool get_keep_thread() const { return (_state_flags & KEEP_THREAD) != 0; } 
	inline bool get_keep_scheduler() const { return (_state_flags & KEEP_SCHEDULER) != 0; }
	
	inline void set_keep_thread(bool b) {
		if (b) _state_flags |= KEEP_THREAD; 
		else   _state_flags &= ~KEEP_THREAD;
	}
		
	inline void set_keep_scheduler(bool b) {
		if (b) _state_flags |= KEEP_SCHEDULER; 
		else   _state_flags &= ~KEEP_SCHEDULER;
	}
};

template <class T>
using TaskFunction = std::function<T()>;


/**
 * Class that executes a function returning type T
 */
template <class T>
class FunctionTask : public Task
{
private:
	TaskFunction<T>             _function;
	T                          *_result;
	
protected:
	virtual void do_run() override {
		assert(_result == nullptr);
		_result = new T(_function());
	}

	FunctionTask(const TaskFunction<T> &fun, Priority prio)
		: Task(prio), _function(fun), _result(nullptr)
	{
	}
	
	virtual ~FunctionTask() { 
		if (_result != nullptr) delete _result;
	}

public:
	inline T get() const {
		assert(state() == COMPLETED);
		return *_result;
	}
	
	static FunctionTask* create(const TaskFunction<T> &fun, Priority prio) {
		return new FunctionTask(fun, prio);
	}
};


/**
 * Specialiation for void 
 */
template <>
class FunctionTask<void> : public Task
{
protected:
	TaskFunction<void>          _function;
	
	FunctionTask(const TaskFunction<void> &fun, Priority prio)
		: Task(prio), _function(fun)
	{
	}
		
	virtual ~FunctionTask() {}
	
	virtual void do_run() override {
		_function(); 
	}

public:
	
	static FunctionTask* create(const TaskFunction<void> &fun, Priority prio) {
		return new FunctionTask(fun, prio);
	}
};

}

template <class T>
using TaskRef = numa::RefPtr<tasking::FunctionTask<T>>;

}

