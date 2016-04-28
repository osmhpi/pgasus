#include "tasking/task_collection.hpp"


using namespace numa::msource;


namespace numa {
namespace tasking {

TaskCollection* TaskCollection::create(const numa::MemSource &alloc, size_t max_threads) {
	void *mem = alloc.alloc(sizeof(TaskCollection));
	return new (mem) TaskCollection(alloc, max_threads);
}

TaskCollection::TaskCollection(const numa::MemSource &alloc, size_t max_threads)
	: _alloc(alloc)
	, _global_tasks(alloc)
	, _thread_tasks(max_threads, alloc)
{
}

TaskCollection::~TaskCollection() {
}

/**
 * Make sure a thread task queue exists for given threa id
 */
void TaskCollection::register_thread(size_t idx) {
	assert(idx < _thread_tasks.size());
	_thread_tasks[idx].create(_alloc);
}

/**
 * delete queue for given threa id
 */
void TaskCollection::deregister_thread(size_t idx) {
	assert(idx < _thread_tasks.size());
	
	TaskQueue *tq = _thread_tasks[idx].queue.load();
	_thread_tasks[idx].queue = nullptr;
	
	// move tasks to global queue
	Task *task = nullptr;
	while (tq->try_pop_front(task)) {
		_global_tasks.push_back(task);
	}
	
	// delete queue
	// stop: we cant do this, as other threads might still be accessing it.
	// we thus decide to maliciously leak memory here. this will
	// happen only when WorkerThreads are removed, i.e. almost never, so
	// we are fine with that :)
	//numa::MemSource::destruct(tq);
}

/**
 * Try to get a thread from the collection.
 */
Task* TaskCollection::try_get(size_t th_idx) {
	
	Task *task = nullptr;
	
	// try to find task. first thread-specific
	try_get_thread_task(th_idx, &task);
	
	// then search global
	if (task == nullptr)
		_global_tasks.try_pop_front(task);
	
	// then try to steal from other threads. randomly to prevent imbalance.
	if (task == nullptr) {
		size_t cnt = _thread_tasks.size();
		
		if (cnt > 0) {
			size_t start = _random() % cnt;
			for (size_t i = start; i < start+cnt; i++) {
				size_t idx = i;
				if (idx >= cnt) idx -= cnt;
				if (try_get_thread_task(idx, &task))
					break;
			}
		}
	}

	return task;
}

/**
 * Inserts the task into the collection.
 */
void TaskCollection::put(Task* t, size_t th_idx) {
	TaskQueue *dst = get_thread_queue(th_idx);
	if (dst == nullptr)
		dst = &_global_tasks;
	dst->push_back(t);
}


}
}
