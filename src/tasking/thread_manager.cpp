#include <algorithm>
#include <cassert>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>

#include "base/node.hpp"
#include "msource/msource.hpp"
#include "msource/msource_allocator.hpp"

#include "tasking/thread_manager.hpp"


namespace numa {
namespace tasking {


/**
 * A class that is managed by a thread manager
 */
ThreadBase::ThreadBase(const MemSource &ms)
	: _msource(ms)
	, _cpuid(-1)
	, _state(CREATED)
	, _manager(nullptr)
{
}

ThreadBase::~ThreadBase() {
	assert(_state == FLOATING);
}

/** Associate with given manager. Can only start if associated. */
void ThreadBase::associate(ThreadManager *mgr, int cpuid) {
	std::lock_guard<LockType> lock(_mutex);

	assert(state() == CREATED);

	_manager = mgr;
	_cpuid = cpuid;

	set_state(ASSOCIATED);
}

/** De-associate from manager. floats freely within the void now. */
void ThreadBase::release(ThreadManager *mgr) {
	std::lock_guard<LockType> lock(_mutex);

	_manager = nullptr;

	assert(state() == TERMINATED);
	set_state(FLOATING);
}

#define ASSERT_SUCCESS(call) do { \
	int ret = call; \
	if (ret != 0) { \
		errno = ret; perror(#call); assert(0); \
	} } while (0)


/** Start actual OS thread on specified cpu. */
void ThreadBase::start() {
	std::lock_guard<LockType> lock(_mutex);

	assert(state() == ASSOCIATED);

	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
	CPU_SET(_cpuid, &cpu_set);

	set_state(RUNNING);

	pthread_attr_t attr;
	ASSERT_SUCCESS(pthread_attr_init(&attr));
	ASSERT_SUCCESS(pthread_attr_setaffinity_np(&attr, sizeof(cpu_set), &cpu_set));
	ASSERT_SUCCESS(pthread_create(&_thread_handle, &attr, thread_func, (void*) this));
	ASSERT_SUCCESS(pthread_attr_destroy(&attr));
}

/** Waits for the thread to terminate */
void ThreadBase::join() {
	assert (state() == RUNNING || state() == TERMINATED || state() == FLOATING);

	void *ret = nullptr;
	if (pthread_join(_thread_handle, &ret) != 0 ) {
		assert(false);
	}
	assert(ret == (void*) this);
}

void* ThreadBase::thread_func(void *arg) {
	ThreadBase *thread = (ThreadBase*) arg;

	assert(thread->state() == RUNNING);

	thread->run();

	thread->set_state(TERMINATED);

	return arg;
}


/**
 * Manages starting, shutting down and waiting on threads that run on
 * a specified cpu set. May have more than one cpu. Bound to a specific node.
 */
ThreadManager::ThreadManager(Node node, const CpuSet& cpuset, const MemSource &ms)
	: _node(node)
	, _msource(ms.valid() ? ms : MemSource::forNode(node))
	, _cpu_set(_msource)
	, _cpu_to_idx(_msource)
	, _cpu_threads(_msource)
{
	assert(node.valid());
	assert(_msource.valid());
	assert(cpuset.size() > 0);

	// setup cpu mapping data structs
	for (size_t i = 0; i < cpuset.size(); i++) {
		_cpu_set.push_back(cpuset[i]);
		_cpu_to_idx[cpuset[i]] = i;
		_cpu_threads.push_back(CpuThreadList(_msource));
	}
}

ThreadManager::~ThreadManager() {
	// make sure there are no more threads
	assert(std::all_of(_cpu_threads.begin(), _cpu_threads.end(),
		std::bind(&mslist<ThreadBase*>::empty, std::placeholders::_1)));
}

bool ThreadManager::manages_thread(ThreadBase *thread) {
	std::lock_guard<std::recursive_mutex> lock(_mutex);
	for (auto &l : _cpu_threads)
		if (std::count(l.begin(), l.end(), thread) > 0) return true;
	return false;
}

/**
 * Register given thread with manager. If cpuid<0, automatically choose
 * target CPU
 */
int ThreadManager::register_thread(ThreadBase *thread, int core) {
	std::lock_guard<std::recursive_mutex> lock(_mutex);

	// find best spot?
	if (core < 0) {
		core = 0;
		for (size_t i = 1; i < _cpu_threads.size(); i++)
			if (_cpu_threads[i].size() < _cpu_threads[core].size())
				core = i;
	}

	assert(core >= 0 && core < (int)_cpu_threads.size());

	_cpu_threads[core].push_back(thread);

	thread->associate(this, _cpu_set[core]);
	thread->start();

	return core;
}

/**
 * De-register thread from manager. Wait for termination if needed.
 */
void ThreadManager::deregister_thread(ThreadBase *thread) {
	std::lock_guard<std::recursive_mutex> lock(_mutex);

	assert(thread != nullptr && manages_thread(thread));
	assert(_cpu_to_idx.count(thread->cpuid()) > 0);

	thread->join();
	thread->release(this);

	int core = _cpu_to_idx[thread->cpuid()];
	_cpu_threads[core].remove(thread);
}

/**
 * Waits for all thread's completion and removes them from the thread manager
 */
void ThreadManager::deregister_all() {
	std::lock_guard<std::recursive_mutex> lock(_mutex);

	for (auto &l : _cpu_threads) {
		for (ThreadBase *tb : l) {
			tb->join();
			tb->release(this);
		}
		l.clear();
	}
}

/**
 * Wait for all thread's completion
 */
void ThreadManager::wait_for_all() {
	std::lock_guard<std::recursive_mutex> lock(_mutex);

	for (auto &l : _cpu_threads) {
		for (ThreadBase *tb : l) {
			tb->join();
		}
	}
}


}
}
