#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <random>
#include <vector>

#include "PGASUS/base/spinlock.hpp"
#include "PGASUS/msource/msource_types.hpp"
#include "PGASUS/hpinuma_export.h"
#include "PGASUS/synced_containers.hpp"


namespace numa {
namespace tasking {

class Task;


/**
 * All tasks that are to run on a number of threads. Contains a local thread
 * queue for each thread, as well as a global queue of untied threads.
 */
class HPINUMA_EXPORT TaskCollection
{
private:
	typedef numa::util::SyncDeque<Task*, numa::MemSourceAllocator<Task*>> TaskQueue;
	typedef numa::SpinLock LockType;

	struct TaskQueueEntry {
		std::atomic<TaskQueue*> queue;
		LockType lock;

		TaskQueueEntry() : queue(nullptr) {}
		TaskQueueEntry(const TaskQueueEntry &other) = delete;
		TaskQueueEntry(TaskQueueEntry &&other) = delete;

		~TaskQueueEntry() {
			numa::MemSource::destructNoRef(queue.load());
		}

		inline void create(numa::MemSource &ms) {
			if (queue.load() == nullptr) {
				std::lock_guard<LockType> guard(lock);
				if (queue.load() == nullptr)
					queue = ms.construct<TaskQueue>(ms);
			}
		}
	};

	typedef numa::msvector<TaskQueueEntry> ThreadTaskQueue;

	numa::MemSource             _alloc;

	TaskQueue                   _global_tasks;
	ThreadTaskQueue             _thread_tasks;

	std::random_device          _random;

	TaskCollection(const numa::MemSource &alloc, size_t max_threads);

	/**
	 * Return given thread task queue, or null of not exists
	 */
	inline TaskQueue *get_thread_queue(size_t idx)  {
		return (idx < _thread_tasks.size()) ? _thread_tasks[idx].queue.load() : nullptr;
	}

	/**
	 * try to get task from task queue associated with given index
	 */
	inline bool try_get_thread_task(size_t idx, Task **task) {
		TaskQueue *tq = get_thread_queue(idx);
		return (tq != nullptr) ? tq->try_pop_front(*task) : false;
	}

public:

	static TaskCollection* create(const numa::MemSource &alloc, size_t max_threads);
	~TaskCollection();

	/**
	 * Make sure a thread task queue exists for given threa id
	 */
	void register_thread(size_t idx);

	/**
	 * delete queue for given threa id, move jobs to global queue
	 */
	void deregister_thread(size_t idx);

	/**
	 * Try to get a thread from the collection.
	 */
	Task* try_get(size_t th_idx);

	/**
	 * Inserts the task into the collection.
	 */
	void put(Task* t, size_t th_idx);
};


}
}

