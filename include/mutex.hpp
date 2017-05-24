#pragma once

#include <atomic>
#include "hpinuma_export.h"
#include "tasking/tasking.hpp"

namespace numa {

class HPINUMA_EXPORT Mutex
{
private:
	/**
	 * If spinlock could not be acquired, wait for this
	 */
	struct HPINUMA_EXPORT WaitObject : public Triggerable
	{
		WaitObject()
		{
		}
		
		virtual ~WaitObject() {
		}
		
		bool locked = false;
		
		/**
		 * Does the client have to wait?
		 * Guaranteed to be called with held lock.
		 */
		virtual bool must_wait(Synchronizer *t) override {
			bool was_locked = locked;
			locked = true;
			
			return was_locked;
		}
		
		void release() {
			std::lock_guard<Triggerable::LockType> lock(_mutex);
			bool triggered = trigger_one();
			
			if (!triggered)
				locked = false;
		}
	};
	
	numa::RefPtr<WaitObject>          _wait;
	
public:
	Mutex()
		: _wait(new WaitObject())
	{
	}
	
	Mutex(const Mutex&) = delete;
	Mutex(Mutex&&) = delete;
	Mutex& operator=(const Mutex&) = delete;
	Mutex& operator=(Mutex&&) = delete;
	
	~Mutex() {
	}
	
	void lock() {
		numa::wait(_wait);
	}
	
	void unlock() {
		_wait->release();
	}
};

}
