#pragma once

#include <atomic>
#include <mutex>
#include <cassert>

#include "PGASUS/base/PGASUS_base_export.h"

namespace numa {

namespace detail {
	template <bool NeedsWriteAccess>
	struct GuardOperation {
	};
}

class PGASUS_BASE_EXPORT RWLock {
private:
	typedef uint_least64_t BaseType;
	typedef std::atomic<BaseType> AtomicType;
	
	AtomicType                      value;
	static constexpr BaseType       FLAG_WRITE = 0x40000000LL;
	
	static constexpr BaseType       BackoffWrite = 4096;
	static constexpr BaseType       BackoffRead = 256;
	
	static inline void backoff(size_t sz) {
		for (size_t i = 0; i < sz; i++) {
			asm("nop");
		}
	}

	static inline void fail(BaseType val, const char *msg) {
		fprintf(stderr, "RWLock: %s (v=%zu)\n", msg, val);
		assert(false);
	}
	
	inline void read_lock() {
		BaseType oldV = value.load();
		
		for (;;) {
			if ((oldV & FLAG_WRITE) != 0) {
				backoff(BackoffWrite);
				oldV = value.load();
			}
			else if (value.compare_exchange_weak(oldV, oldV+1))
				break;
		}
		BaseType v = value.load();
		if (v == 0) fail(v, "Must be positive");
		if (v == FLAG_WRITE) fail(v, "Write Flag must not be set");
	}
	
	inline void read_unlock() {
		BaseType v = value.load();
		if (v == 0) fail(v, "Must be positive");
		if (v == FLAG_WRITE) fail(v, "Write Flag must not be set");
		value -= 1;
	}
	
	inline void write_lock() {
		// set WRITE flag
		BaseType oldV = value.load();
		
		for (;;) {
			if ((oldV & FLAG_WRITE) != 0) {
				backoff(BackoffWrite);
				oldV = value.load();
			}
			else if (value.compare_exchange_weak(oldV, oldV | FLAG_WRITE))
				break;
		}
		
		// wait until there are no more readers
		while (value.load() != FLAG_WRITE)
			backoff(BackoffRead);
		assert(value.load() == FLAG_WRITE);
	}
	
	inline void write_unlock() {
		assert(value.load() == FLAG_WRITE);
		value.store(0);
	}

public:
	inline RWLock() : value(0) {}
	
	RWLock(const RWLock&) = delete;
	RWLock(RWLock&&) = delete;
	
	RWLock& operator=(const RWLock&) = delete;
	RWLock& operator=(RWLock&&) = delete;
	
	struct ReadGuard {
	private:
		RWLock *ref;

	public:
		explicit inline ReadGuard(RWLock *r) : ref(r) {
			if (ref != nullptr) ref->read_lock();
		}
		
		inline ReadGuard(const ReadGuard &other) : ref(other.ref) {
			if (ref != nullptr) ref->read_lock();
		}
		
		inline ReadGuard(ReadGuard &&other) : ref(other.ref) {
			other.ref = nullptr;
		}
		
		void operator=(const ReadGuard&) = delete;
		void operator=(ReadGuard&&) = delete;
		
		inline ~ReadGuard() {
			if (ref != nullptr) ref->read_unlock();
		}
	};
	
	struct WriteGuard {
	private:
		RWLock *ref;

	public:
		explicit inline WriteGuard(RWLock *r) : ref(r) {
			if (ref != nullptr) ref->write_lock();
		}
		
		inline WriteGuard(const WriteGuard &other) : ref(other.ref) {
			if (ref != nullptr) ref->write_lock();
		}
		
		inline WriteGuard(WriteGuard &&other) : ref(other.ref) {
			other.ref = nullptr;
		}
		
		void operator=(const WriteGuard&) = delete;
		void operator=(WriteGuard&&) = delete;
		
		inline ~WriteGuard() {
			if (ref != nullptr) ref->write_unlock();
		}
	};
	
	inline WriteGuard write_guard() {
		return WriteGuard(this);
	}
	
	inline ReadGuard read_guard() {
		return ReadGuard(this);
	}
	
	template <bool NeedsWriteAccess>
	inline typename detail::GuardOperation<NeedsWriteAccess>::return_type guard() {
		return detail::GuardOperation<NeedsWriteAccess>::guard(*this);
	}
};

namespace detail {
	template <>
	struct GuardOperation<true> {
		typedef RWLock::WriteGuard return_type;
		static inline return_type guard(RWLock &lock) {
			return lock.write_guard();
		}
	};
	
	template <>
	struct GuardOperation<false> {
		typedef RWLock::ReadGuard return_type;
		static inline return_type guard(RWLock &lock) {
			return lock.read_guard();
		}
	};
}

}
