#pragma once

#include <mutex>

#include "PGASUS/PGASUS_export.h"
#include "PGASUS/msource/msource_types.hpp"


namespace numa {
namespace tasking {


class ThreadManager;


/**
 * A class that is managed by a thread manager
 */
class PGASUS_EXPORT ThreadBase {
private:
	enum State {
		CREATED, 
		ASSOCIATED,
		RUNNING, 
		TERMINATED,
		FLOATING
	};
	
	typedef std::recursive_mutex LockType;
	
	numa::MemSource             _msource;
	
	pthread_t                   _thread_handle;
	CpuId                       _cpuid;	// bound to cpu!
	
	LockType                    _mutex;
	State                       _state;
	
	ThreadManager              *_manager;
	
	static void* thread_func(void *arg);
	
	inline State state() {
		std::lock_guard<LockType> lock(_mutex);
		return _state;
	}
	
	inline void set_state(State t) {
		std::lock_guard<LockType> lock(_mutex);
		_state = t;
	}

protected:
	void sleep();
	
	const numa::MemSource& msource() const { return _msource; }

public:
	explicit ThreadBase(const numa::MemSource &ms);
	virtual ~ThreadBase();
	
	/** Associate with given manager. Can only start if associated. */
	void associate(ThreadManager *mgr, CpuId cpuid);
	
	/** De-associate from manager. floats freely within the void now. */
	void release(ThreadManager *mgr);
	
	/** Start actual OS thread on specified cpu. */
	void start();
	
	/** Waits for the thread to terminate */
	void join();
	
	/** Returns cpu on which the thread is running */
	CpuId cpuid() const { return _cpuid; }
	
	virtual void run() = 0;
};

/**
 * Manages starting, shutting down and waiting on threads that run on
 * a specified cpu set. May have more than one cpu. Bound to a specific node.
 */
class PGASUS_EXPORT ThreadManager {
private:

	template <class T> using msvector = numa::msvector<T>;
	template <class T> using mslist = numa::mslist<T>;
	template <class K, class V> using msmap = numa::msmap<K,V>;
	
	// number of threads running on given cpu
	typedef mslist<ThreadBase*> CpuThreadList;
	
	Node                        _node;
	numa::MemSource             _msource;
	
	msvector<int>               _cpu_set;
	msmap<int,int>              _cpu_to_idx;
	
	// fully synchronized
	std::recursive_mutex        _mutex;
	msvector<CpuThreadList>     _cpu_threads;
	
public:
	
	ThreadManager(Node node, const CpuSet& cpuset, const numa::MemSource &ms);
	~ThreadManager();

	static ThreadManager* create(Node node, const CpuSet& cpuset, const numa::MemSource &ms) {
		return ms.construct<ThreadManager>(node, cpuset, ms);
	}
	
	static void destroy(ThreadManager *mgr) {
		numa::MemSource::destruct(mgr);
	}
	
	CpuSet cpu_set() const {
		CpuSet set;
		std::copy(_cpu_set.begin(), _cpu_set.end(), set.begin());
		return set;
	}
	
	/** 
	 * Register given thread with manager. If core<0, automatically choose 
	 * target CPU. Returns the core where the thread was inserted.
	 * Also starts the thread.
	 */
	int register_thread(ThreadBase *thread, int core = -1);
	
	/**
	 * De-register thread from manager. Wait for termination if needed.
	 */
	void deregister_thread(ThreadBase *thread);
	
	/**
	 * Wait for all thread's completion
	 */
	void wait_for_all();
	
	/**
	 * Waits for all thread's completion and removes them from the thread manager
	 */
	void deregister_all();
	
	bool manages_thread(ThreadBase *thread);
	
};


}
}
