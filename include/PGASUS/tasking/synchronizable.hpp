#pragma once

#include <list>
#include <mutex>

#include "PGASUS/base/spinlock.hpp"
#include "PGASUS/base/ref_ptr.hpp"
#include "PGASUS/PGASUS_export.h"

namespace numa {

class Triggerable;
class Synchronizer;

/** 
 * Some entity upon whose completion can be waited by a thread or a task.
 * Is in the triggered or non-triggered state. If in non-triggered state,
 * clients wanting to synchronize with this object will be registered and 
 * notified, when the objet has finally triggered.
 */
class PGASUS_EXPORT Triggerable : public numa::Referenced {
protected:
	std::list<Synchronizer*>                _clients;

	typedef numa::SpinLock LockType;
	LockType                                _mutex;
	
protected:
	Triggerable() {
	}

	virtual ~Triggerable() {
		assert(_clients.empty());
	}
	
	/** 
	 * Signals one waiting client. Expects the lock to be held.
	 * Returns, if there was a waiting client.
	 */
	inline bool trigger_one();
	
	/**
	 * Signals all waiting clients. Expects the lock to be held.
	 */
	inline int trigger_all();
	
	/**
	 * Does the client have to wait?
	 * Guaranteed to be called with held lock.
	 */
	virtual bool must_wait(Synchronizer *t) = 0;

public:
	
	/** 
	 * Registers the given client as waiting for this entity.
	 * Returns true, if the client has to wait.
	 * Returns false, if the client does not have to wait.
	 */
	inline bool register_wait(Synchronizer *t) {
		std::lock_guard<LockType> lock(_mutex);
		if (must_wait(t)) {
			_clients.push_back(t);
			return true;
		}
		return false;
	}
};

using TriggerableRef = numa::RefPtr<Triggerable>;

/**
 * Specialized Triggerable that is initialized unsignaled, then gets signaled.
 * Base for tasks, etc. that change their state exactly once
 */
class PGASUS_EXPORT TwoPhaseTriggerable : public Triggerable
{
private:
	bool        _state;

protected:
	virtual bool must_wait(Synchronizer *sync) override {
		return !_state;
	}
	
	void set_signaled() {
		std::lock_guard<Triggerable::LockType> lock(_mutex);
		assert(!_state);
		_state = true;
		trigger_all();
	}
	
	TwoPhaseTriggerable()
		: _state(false)
	{
	}
	
	virtual ~TwoPhaseTriggerable() {
	}
};


/**
 * An object that may wait for the completion of a triggerable entity.
 * Keep references to that item to make sure that it is not deleted.
 */
class PGASUS_EXPORT Synchronizer {
private:
	typedef numa::SpinLock LockType;
	
	LockType                    _mutex;
	std::list<TriggerableRef>   _dependancies;
	
	friend class Triggerable;
	
	/**
	 * Gets called by waitable object that has finished 
	 */
	inline void signal(const TriggerableRef &ref) {
		std::lock_guard<LockType> lock(_mutex);
		
		assert(!_dependancies.empty());
		
		_dependancies.remove(ref);
		
		// not waiting any longer?
		if (_dependancies.empty()) {
			notify();
		}
	}

protected:
	virtual void notify() = 0;

public:
	Synchronizer() {}
	~Synchronizer() {}
	
	/**
	 * Is currently waiting?
	 */
	inline bool is_waiting() {
		std::lock_guard<LockType> lock(_mutex);
		return !_dependancies.empty();
	}
	
	/**
	 * Wait for Waitable to complete. Returns true, if waiting.
	 */
	inline bool synchronize(const TriggerableRef &ref) {
		std::lock_guard<LockType> lock(_mutex);
		
		if (ref->register_wait(this)) {
			_dependancies.push_back(ref);
		}
		
		return !_dependancies.empty();
	}
	
	/**
	 * Wait for a number of Waitables to complete. Returns true, if waiting.
	 */
	template <class T>
	inline bool synchronize(const T &list) {
		std::lock_guard<LockType> lock(_mutex);
		
		for (const TriggerableRef &ref : list) {
			if (ref->register_wait(this)) {
				_dependancies.push_back(ref);
			}
		}
		
		return !_dependancies.empty();
	}
};

/** 
 * Signals one waiting client. Expects the lock to be held.
 */
inline bool Triggerable::trigger_one() {
	if (!_clients.empty()) {
		Synchronizer *sync = _clients.front();
		_clients.pop_front();
		sync->signal(this);
		return true;
	}
	return false;
}

/**
 * Signals all waiting clients. Expects the lock to be held.
 */
inline int Triggerable::trigger_all() {
	size_t n = _clients.size();
	for (auto sync : _clients)
		sync->signal(this);
	_clients.clear();
	return n;
}
	
}
