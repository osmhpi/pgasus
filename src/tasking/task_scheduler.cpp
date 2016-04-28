#include <cassert>

#include "msource/node_replicated.hpp"
#include "msource/msource_types.hpp"

#include "tasking/task_scheduler.hpp"
#include "tasking/worker_thread.hpp"
#include "tasking/thread_manager.hpp"

#include "util/topology.hpp"
#include "util/debug.hpp"
#include "util/timer.hpp"


namespace numa {
namespace tasking {

struct GlobalInitializer {
	SchedulingDomain domain;
	NodeReplicated<Scheduler> *schedulers;

	GlobalInitializer()
		: domain(MemSource::global())
	{
		schedulers = MemSource::global().construct<NodeReplicated<Scheduler>>();
	}
	~GlobalInitializer() {
		MemSource::destruct(schedulers);
	}
};

static GlobalInitializer& initer() {
	static GlobalInitializer gi;
	return gi;
}

static NodeReplicated<Scheduler>& getNodeSchedulers() {
	return *initer().schedulers;
}

static SchedulingDomain* globalDomain() {
	return &initer().domain;
}

/**
 * Encapsulates all priorities within one scheduling domain
 */
SchedulingDomain::SchedulingDomain(const MemSource &ms)
	: _msource(ms.valid() ? ms : MemSource::global())
	, _active_thread_ids(_msource)
	, _topPriorityIdx(0)
	, _priorities(Priority::max_index() + 1, _msource)
{
}

SchedulingDomain::~SchedulingDomain() {
	for (PriorityTasks &pt : _priorities)  {
		MemSource::destructNoRef(pt.tasks.load());
	}
}

/**
 * Returns a task ready for execution. If there is no such task, return null.
 * Always picks the highest-priority task. Prefers tasks bound to that thid
 */
Task* SchedulingDomain::try_get_task(int thid) {
	for (ssize_t idx = _topPriorityIdx.load(); idx >= 0; --idx) {
		if (_priorities[idx].count.load() > 0) {
			Task *result = _priorities[idx].tasks.load()->try_get(thid);
			if (result != nullptr) {
				_priorities[idx].count -= 1;
				return result;
			}
		}
	}
	return nullptr;
}

/**
 * Inserts a task into this scheduling domain
 */
void SchedulingDomain::put_task(Task* t, int thid) {
	size_t idx = t->priority().index();

	// create lazy, if necessary
	if (_priorities[idx].tasks.load() == nullptr) {
		std::lock_guard<Lock> lock(_priorities[idx].mutex);

		if (_priorities[idx].tasks.load() == nullptr) {
			// get number of CPUs on this node, or prepare for all CPUs on machine
			const util::Topology *topo = util::Topology::get();
			int physNode = _msource.getPhysicalNode();
			size_t cpuCount = (physNode >= 0) ? topo->get_node(physNode)->cpus.size()
											  : topo->max_cpu_id() + 1;

			TaskCollection *tc = TaskCollection::create(_msource, cpuCount);
			_priorities[idx].count = 0;

			// add worker threads
			std::lock_guard<Lock> lock(_active_thread_ids_mutex);
			for (auto thid : _active_thread_ids)
				tc->register_thread(thid);

			_priorities[idx].tasks = tc;
		}
	}

	_priorities[idx].tasks.load()->put(t, thid);
	_priorities[idx].count += 1;

	// update priority search head
	size_t expected = _topPriorityIdx.load();
	while (idx > expected) {
		if (_topPriorityIdx.compare_exchange_weak(expected, idx)) break;
	}
}

/** Adds given thread ID to task collections */
void SchedulingDomain::add_thread(int idx) {
	for (auto &p : _priorities) p.mutex.lock();
	_active_thread_ids_mutex.lock();
	
	for (auto &p : _priorities) {
		if (p.tasks.load() != nullptr)
			p.tasks.load()->register_thread(idx);
	}
	
	_active_thread_ids.push_back(idx);
	
	_active_thread_ids_mutex.unlock();
	for (auto &p : _priorities) p.mutex.unlock();
}

/** Removes given thread ID from task collections */
void SchedulingDomain::remove_thread(int idx) {
	for (auto &p : _priorities) p.mutex.lock();
	_active_thread_ids_mutex.lock();
	
	for (auto &p : _priorities) {
		if (p.tasks.load() != nullptr)
			p.tasks.load()->deregister_thread(idx);
	}
	
	// delete index from list
	auto it = _active_thread_ids.begin();
	while (it != _active_thread_ids.end()) {
		if (*it == idx) {
			_active_thread_ids.erase(it);
			break;
		}
		++it;
	}
	assert(it != _active_thread_ids.end());
	
	_active_thread_ids_mutex.unlock();
	for (auto &p : _priorities) p.mutex.unlock();
}



Scheduler::Scheduler(const Node &node)
	: _node(node)
	, _msource(MemSource::forNode(node))
	, _domain(_msource.construct<SchedulingDomain>(_msource))
	, _workers(_msource)
	, _ctx_cache(_msource)
{
	std::vector<CpuId> cpus = node.cpuids();
	_cores = cpus.size();
	_workers.resize(_cores, nullptr);

	_waitingThreadsCount = 0;
	assert(sem_init(&_waitingThreadsSemaphore, 0, 0) == 0);

	_thread_manager = ThreadManager::create(_node, cpus, _msource);
	set_thread_count(node.threadCount());
}

Scheduler::~Scheduler() {
	// save so that we can delete them later
	msvector<WorkerThread*> tmpworkers(_msource);

	// delete workers
	{
		std::lock_guard<std::recursive_mutex> wlock(_workers_lock);
		tmpworkers.insert(tmpworkers.begin(), _workers.begin(), _workers.end());

		// Notify all threads to stop
		for (int i = 0; i < _cores; i++) {
			_workers[i] = nullptr;
			if (tmpworkers[i] != nullptr) {
				_domain->remove_thread(i);
				tmpworkers[i]->shutdown();
				tmpworkers[i]->notify();
			}
		}
	}

	// notify all workers, if they are sleeping
	for (size_t i = 0; i < _workers.size(); i++)
		taskAvailable();

	// shutdown thread manager; wait for threads to complete
	_thread_manager->deregister_all();
	ThreadManager::destroy(_thread_manager);

	// destroy thread objects
	for (WorkerThread *wt : tmpworkers) {
		MemSource::destruct(wt);
	}

	MemSource::destruct(_domain);

	assert(sem_destroy(&_waitingThreadsSemaphore) == 0);
}

Scheduler* Scheduler::get_scheduler(const Node &node) {
	return &getNodeSchedulers().get(node);
}

/**
 * Create a new thread on the given core, if none is yet running there.
 * Return the new thread, or NULL, if there was another thread already
 */
WorkerThread* Scheduler::create_thread(int core) {
	std::lock_guard<std::recursive_mutex> lock(_workers_lock);
	
	assert(core >= 0 && core < _cores);
	assert(_workers[core] == nullptr);
	
	WorkerThread *th = _msource.construct<WorkerThread>(core, this, _msource);
	
	_domain->add_thread(core);
	_workers[core] = th;
	
	_thread_manager->register_thread(th, core);
	
	return th;
}

/**
 * Remove thread running on given node core. Waits until the thread has
 * terminated.
 */
void Scheduler::stop_wait_thread(int core) {
	std::lock_guard<std::recursive_mutex> wlock(_workers_lock);
	
	assert(core >= 0 && core < _cores);
	assert(_workers[core] != nullptr);
	
	// remove from store
	WorkerThread *th = _workers[core];
	_workers[core] = nullptr;
	_domain->remove_thread(core);
	
	// shutdown thread
	th->shutdown();
	th->notify();
	
	// wait for completion
	_thread_manager->deregister_thread(th);
	
	// destroy object
	MemSource::destruct(th);
}

/**
 * Sets worker thread by core IDs
 */
void Scheduler::set_threads(const std::vector<int> &core_ids) {
	std::lock_guard<std::recursive_mutex> lock(_workers_lock);
	
	// cpuset list -> bitmap
	std::vector<bool> oncore(_cores, false);
	for (int c : core_ids) {
		assert(c >= 0 && c < _cores);
		oncore[c] = true;
	}
	
	for (int c = 0; c < _cores; c++) {
		// create thread?
		if (core_ids[c] && _workers[c] == nullptr) {
			create_thread(c);
		}
		
		// or remove thread
		else if (!core_ids[c] && _workers[c] != nullptr) {
			stop_wait_thread(c);
		}
	}
}

/**
 * Sets worker thread count 
 */
void Scheduler::set_thread_count(int count) {
	std::lock_guard<std::recursive_mutex> lock(_workers_lock);
	
	assert(count >= 0 && count <= _cores);
	
	// count current
	int curr = 0;
	for (WorkerThread *w : _workers) if (w != nullptr) curr += 1;
	
	// too few?
	for (int c = 0; (c < _cores) && (curr < count); c++) {
		if (_workers[c] == nullptr) {
			create_thread(c);
			curr += 1;
		}
	}
	
	// too many?
	for (int c = _cores-1; (c >= 0) && (curr > count); c--) {
		if (_workers[c] != nullptr) {
			stop_wait_thread(c);
			curr -= 1;
		}
	}
}

/**
 * Returns IDs of all workers
 */
std::vector<int> Scheduler::worker_ids() {
	std::lock_guard<std::recursive_mutex> lock(_workers_lock);

	std::vector<int> ret;
	ret.reserve(_workers.size());

	for (WorkerThread *w : _workers)  {
		if (w != nullptr)
			ret.push_back(w->id());
	}

	return ret;
}

/**
 * Wake N threads from their sleep
 */
void Scheduler::taskAvailable() {
	if (_waitingThreadsCount.load() > 0) {
		size_t old = _waitingThreadsCount.exchange(0);

		for (size_t i = 0; i < old; i++) {
			sem_post(&_waitingThreadsSemaphore);
		}
	}
}

/**
 * Wait for a while for a task to be available
 */
void Scheduler::waitForTask(size_t usec) {
	// slow path: register and wait for a while on a semaphore
	_waitingThreadsCount++;

	// get current time
	struct timeval currTime;
	gettimeofday(&currTime, 0);

	// dest. time
	struct timespec waitTime;
	waitTime.tv_sec = currTime.tv_sec;
	waitTime.tv_nsec = 1000 * (currTime.tv_usec + usec);
	if (waitTime.tv_nsec > 1000000000) {
		waitTime.tv_nsec -= 1000000000;
		waitTime.tv_sec += 1;
	}

	sem_timedwait(&_waitingThreadsSemaphore, &waitTime);
}

/**
 * Introduces the given task to scheduling task queues
 */
void Scheduler::spawn_task(Scheduler *sched, Task *task) {
	// global?
	if (sched == nullptr) {
		globalDomain()->put_task(task, -1);
		
		for (const Node &node : NodeList::allNodes())
			getNodeSchedulers().get(node).taskAvailable();
	}
	// local?
	else {
		int thid = -1;
		WorkerThread *th = WorkerThread::curr_worker_thread();
		if (th != nullptr && th->homeNode() == sched->node())
			thid = th->id();
		sched->put_task(task, thid);
	}
}

/**
 * Returns a task ready for execution from local or global scheduling domains
 */
Task* Scheduler::try_get_task(int thid) {
	// fast path: try to get directly
	Task *t = _domain->try_get_task(thid);
	if (t != nullptr)
		return t;
	return globalDomain()->try_get_task(-1);
}

/**
 * Adds task to scheduling task queues. If scheduler is NULL, insert into
 * global task queues. Else into scheduler's queue.
 */
void Scheduler::put_task(Task* t, int thid) {
	taskAvailable();
	_domain->put_task(t, thid);
}

}
}
