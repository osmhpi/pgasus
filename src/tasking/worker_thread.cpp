#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <list>
#include <vector>

#include <semaphore.h>

#include "base/spinlock.hpp"
#include "msource/msource.hpp"
#include "tasking/context.hpp"
#include "tasking/synchronizable.hpp"
#include "tasking/task.hpp"
#include "tasking/task_scheduler.hpp"
#include "tasking/thread_manager.hpp"
#include "tasking/worker_thread.hpp"
#include "util/debug.hpp"
#include "util/timer.hpp"


namespace numa {
namespace tasking {


using numa::debug::log;
using numa::debug::DebugLevel;

thread_local std::atomic_uintptr_t self_ptr;

/**
 * Sets the TLS to the given thread
 */
void WorkerThread::set_tls(WorkerThread *th) {
	self_ptr = reinterpret_cast<uintptr_t>(th);
}

/**
 * Returns this worker thread. Returns null, if running thread is not a
 * registered worker thread
 */
WorkerThread* WorkerThread::curr_worker_thread() {
	return reinterpret_cast<WorkerThread*>(self_ptr.load());
}


/**
 * A WorkerThread represents a running thread that executes Tasks that it
 * receives from the associated Scheduler
 */
WorkerThread::WorkerThread(size_t id, Scheduler *sched, const MemSource &ms)
	: ThreadBase(ms)
	, _scheduler(sched)
	, _thread_id(id)
	, _node(sched->node())
	, _curr_task(nullptr)
	, _curr_ctx(nullptr)
	, _ready_contexes(msource())
{
	if (sem_init(&_sleep, 0, 0) != 0) {
		assert(false);
	}
	_done = 0;
}

void WorkerThread::run() {
	set_tls(this);

#if ENABLE_DEBUG_LOG && !PGASUS_PLATFORM_PPC64LE
	Counter start_cycles = numa::util::rdtsc();		// count cycles
	Timer<int> timer(true);							// count wall-time

	reset_get_delta();
#endif

	// create new neutral context, start execution in that context,
	// saving the root context locally
	_curr_ctx = get_neutral_context();
	_curr_ctx->jump_from(&_native_context, (intptr_t)this);

#if ENABLE_DEBUG_LOG && !PGASUS_PLATFORM_PPC64LE
	int total_time = timer.stop_get();
	Counter total_cycles = numa::util::rdtsc() - start_cycles;

	log(DebugLevel::DEBUG, "WorkerThread spent %d.%03ds: "
		"get_task=%1.2f run=%1.2f sleep=%1.2f unempl=%1.2f taskmgmt(y=%1.2f w=%1.2f s=%1.2f d=%1.2f)",
		total_time / 1000, total_time % 1000,
		(float)_time_getting_task / (float)total_cycles,
		(float)_time_running      / (float)total_cycles,
		(float)_time_sleeping     / (float)total_cycles,
		(float)_time_task_yield   / (float)total_cycles,
		(float)_time_task_wait    / (float)total_cycles,
		(float)_time_task_sched   / (float)total_cycles,
		(float)_time_task_done    / (float)total_cycles,
		(float)_time_unemployment / (float)total_cycles
	);
#endif

	// we are done here
}

WorkerThread::~WorkerThread() {
	for (Context *c : _ready_contexes)
		_scheduler->context_cache().store(c);
	if (sem_destroy(&_sleep) != 0) {
		assert(false);
	}
}

inline Task *WorkerThread::get_new_task() {
	numa::LinearBackOff<256, 2048> bkoff;

	while (_done.load() == 0) {
		Task *t = _scheduler->try_get_task(_thread_id);
		if (t != nullptr)
			return t;

		// wait a while before trying again.
		if (!bkoff()) {
			// if we waited long enough, go to sleep state
			// and be either woken up by scheduler, or by timeout
			_scheduler->waitForTask(10 * 1000);	// 10 msec
			bkoff.reset();
		}
	}

	return nullptr;
}

inline Context *WorkerThread::get_neutral_context() {
	if (!_ready_contexes.empty()) {
		Context *result = _ready_contexes.back();
		_ready_contexes.pop_back();
		return result;
	}
	return _scheduler->context_cache().get(start_new_context);
}

inline void WorkerThread::put_neutral_context(Context *ctx) {
	_ready_contexes.push_back(ctx);
}

void WorkerThread::start_new_context(intptr_t ptr) {
	WorkerThread *self = reinterpret_cast<WorkerThread*>(ptr);

	while (self->_done.load() == 0) {
#if ENABLE_DEBUG_LOG && !PGASUS_PLATFORM_PPC64LE
		self->_time_running += self->reset_get_delta();
#endif

		// if we have a current task that means the task was interrupted.
		// the context of the tasks is stored therein.
		if (self->_curr_task != nullptr) {
			// just yield?
			if (self->_task_waits.empty()) {
				self->_curr_task->yield(self->id());
				self->_curr_task = nullptr;

#if ENABLE_DEBUG_LOG && !PGASUS_PLATFORM_PPC64LE
				self->_time_task_yield += self->reset_get_delta();
#endif
			}
			else {
				if (self->_curr_task->wait(self->_task_waits)) {
					self->_curr_task = nullptr;
				}
				// else just resume task
				self->_task_waits.clear();

#if ENABLE_DEBUG_LOG && !PGASUS_PLATFORM_PPC64LE
				self->_time_task_wait += self->reset_get_delta();
#endif
			}
		}

		// get new task, if the old one was given away
		if (self->_curr_task == nullptr) {
			self->_curr_task = self->get_new_task();

			if (self->_curr_task == nullptr) break; // no new task -> quit
		}

		// start task?
		if (!self->_curr_task->has_started()) {
			self->_curr_task->schedule(self);
#if ENABLE_DEBUG_LOG && !PGASUS_PLATFORM_PPC64LE
			self->_time_task_sched += self->reset_get_delta();
#endif

			self = self->_curr_task->run(self->_curr_ctx);
#if ENABLE_DEBUG_LOG && !PGASUS_PLATFORM_PPC64LE
			self->_time_running += self->reset_get_delta();
#endif
			self->_curr_task->done();
			self->_curr_task->unref();
			self->_curr_task = nullptr;
#if ENABLE_DEBUG_LOG && !PGASUS_PLATFORM_PPC64LE
			self->_time_task_done += self->reset_get_delta();
#endif
		}
		// continue task?
		else {
			{
				self->_curr_task->schedule(self);
#if ENABLE_DEBUG_LOG && !PGASUS_PLATFORM_PPC64LE
				self->_time_task_sched += self->reset_get_delta();
#endif
			}
			self = (WorkerThread*) self->_curr_ctx->jump_to(self->_curr_task->get_context(), self);
		}
	}

#if ENABLE_DEBUG_LOG && !PGASUS_PLATFORM_PPC64LE
	self->_time_running += self->reset_get_delta();
#endif

	// thread was commanded to stop. jump back to native ctx
	self->_curr_ctx->jump_to(&self->_native_context, (void*) 0);
}

/**
 * Pauses the execution of the given task for yielding/waiting reasons.
 * Returns where it left off, when the task gets rescheduled
 */
void WorkerThread::drop_task(WorkerThread *self) {
	// switch to some acquired neutral context, send task information,
	self->_curr_ctx = self->get_neutral_context();
	self = (WorkerThread*) self->_curr_task->get_context()->jump_to(self->_curr_ctx, self);

	// stash away old context
	// TODO: what to do with that in the long term?
	Context *new_ctx = self->_curr_task->get_context();
	assert(self->_curr_ctx != nullptr && self->_curr_ctx != new_ctx);

	self->put_neutral_context(self->_curr_ctx);
	self->_curr_ctx = new_ctx;
}

/**
 * Lets the currently running task wait for the given tasks
 */
void WorkerThread::curr_task_wait(const std::list<TriggerableRef> &tasks) {
	WorkerThread *self = curr_worker_thread();

	assert(self != nullptr);
	assert(self->_curr_task != nullptr);

	self->_task_waits = tasks;

	drop_task(self);
}

/**
 * Lets the currently running task yield (i.e. give up execution to be
 *  scheduled again later).
 */
void WorkerThread::yield() {
	curr_task_wait(std::list<TriggerableRef>());
}

/**
 * Gets called when the thread has registered as unemployed at its
 * local job center
 */
void WorkerThread::notify() {
	if (sem_post(&_sleep) != 0) {
		assert(false);
	}
}

void WorkerThread::shutdown() {
	_done = 1;
}

}
}
