#include <cstddef>
#include <cassert>
#include <list>
#include <mutex>

#include "malloc.hpp"
#include "base/node.hpp"
#include "tasking/synchronizable.hpp"
#include "tasking/task.hpp"
#include "tasking/task_scheduler.hpp"
#include "tasking/worker_thread.hpp"
#include "util/debug.hpp"

using numa::debug::log;
using numa::debug::DebugLevel;


namespace numa {
namespace tasking {

class Context;


/**
 * Create the task, bind it to the given scheduler
 */
Task::Task(Priority prio)
	: _state_flags(READY | KEEP_SCHEDULER)
	, _priority(prio)
	, _scheduler(nullptr)
	, _home_thread(nullptr)
	, _context(nullptr)
{
	ref();
}

Task::~Task() {
	assert(state() == COMPLETED);
	assert(ref_count() == 0);
}

size_t Task::home_thread_id() const {
	return (_home_thread != nullptr) ? _home_thread->id() : (size_t)-1;
}

Node Task::node() const {
	return _scheduler->node();
}

int Task::cpuid() const {
	return _home_thread->cpuid();
}

WorkerThread* Task::run(Context *ctx) {
	{
		std::lock_guard<Lock> lock(_mutex);
		assert (!has_started());
		_state_flags |= HAS_STARTED;
	}
	_context = ctx;
	do_run();

	return _home_thread;

}

Context* Task::get_context() {
	std::lock_guard<Lock> lock(_mutex);
	assert (has_started());
	return _context;
}

/**
 * Notifies the thread of the completion of the given other thread.
 * Returns true if caused state-change away from waiting.
 */
void Task::notify() {
	std::lock_guard<Lock> lock(_mutex);

	assert(state() == WAITING);
	set_state(SUSPENDED);

	_scheduler->put_task(this, home_thread_id());
}

/**
 * Start or continue execution of the task in the given thread.
 * This doesn't inform the scheduler as we expect the scheduler to have it
 * removed already.
 */
void Task::schedule(WorkerThread *th) {
	std::lock_guard<Lock> lock(_mutex);

	assert (th != _home_thread || !get_keep_thread());
	_home_thread = th;
	_scheduler = th->scheduler();

	set_state(RUNNING);

	numa::malloc::push_all(_place_stack);
	_place_stack.clear();

	log(DebugLevel::INFO, "Task[%p]: scheduled by [%2d.%02d]", (void*)this,
		th->scheduler()->node(), th->id());
}

/**
 * Wait for other task to complete. Returns true, if a state change to
 * a waiting state has happened (the other task may have already completed)
 */
bool Task::wait(const TriggerableRef &ref) {
	std::lock_guard<Lock> lock(_mutex);

	if (this->synchronize(ref)) {
		assert (state() == RUNNING);
		set_state(WAITING);

		_place_stack = numa::malloc::pop_all();

		log(DebugLevel::INFO, "Task[%p]: Wait for [%p]", (void*)this, (void*)ref.get());
	}

	return (state() == WAITING);
}

/**
 * Wait for a number of tasks to complete. Returns true, if a state change to
 * a waiting state has happened (the other tasks may have already completed)
 */
bool Task::wait(const std::list<TriggerableRef> &refs) {
	std::lock_guard<Lock> lock(_mutex);

	if (this->synchronize(refs)) {
		assert (state() == RUNNING);
		set_state(WAITING);

		_place_stack = numa::malloc::pop_all();

		log(DebugLevel::INFO, "Task[%p]: Wait for multiple");
	}

	return (state() == WAITING);
}

/**
 * Suspends execution and bringts it back to the scheduler.
 */
void Task::yield(size_t th_idx) {
	std::lock_guard<Lock> lock(_mutex);

	assert (state() == RUNNING);
	set_state(SUSPENDED);

	_place_stack = numa::malloc::pop_all();

	log(DebugLevel::INFO, "Task[%p]: Yield", (void*)this);

	_scheduler->put_task(this, th_idx);
}

/**
 * Marks the task as completed. Informs waiting tasks and threads.
 */
void Task::done() {
	std::lock_guard<Lock> lock(_mutex);

	assert (state() == RUNNING);
	set_state(COMPLETED);

	numa::malloc::pop_all();

	this->set_signaled();

	log(DebugLevel::INFO, "Task[%p]: Done", (void*)this);
}

}
}

