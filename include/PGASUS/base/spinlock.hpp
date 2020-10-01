#pragma once

#include <atomic>
#include <cstring>

#include "PGASUS/PGASUS-config.h"

namespace numa {

namespace
{
	inline void instruction_pause()
	{
#if PGASUS_PLATFORM_PPC64LE
	// http://stackoverflow.com/a/7588941
	// See: IBM POWER ISA v2.07
	// Alternatives: yield, mdoio, mdoom
		// same as asm("mdoom"); (not supported on clang 3.8)
		asm("or 30,30,30");
#elif PGASUS_PLATFORM_S390X
                asm("");
#else
		asm("pause");
#endif
	}
}

template <size_t INITIAL = 16, size_t MAXBKOFF = (1<<10)>
struct ExponentialBackOff {
	size_t curr;
	ExponentialBackOff() : curr(INITIAL) {}

	inline bool operator()() { return backoff(); }

	inline bool backoff() {
		for (size_t i = 0; i < curr; i++)
			instruction_pause();
		if (curr < MAXBKOFF) {
			curr <<= 1;
			return true;
		}
		return false;
	}

	inline void reset() {
		curr = INITIAL;
	}
};

template <size_t STEP = 32, size_t MAX = 1024>
struct LinearBackOff {
	size_t curr;
	LinearBackOff() : curr(STEP) {}

	inline bool operator()() { return backoff(); }

	inline bool backoff() {
		for (size_t i = 0; i < curr; i++)
			instruction_pause();
		if (curr < MAX) {
			curr += STEP;
			return true;
		}
		return false;
	}

	inline void reset() {
		curr = STEP;
	}
};

#ifndef NUMA_PROFILE_SPINLOCK
#define NUMA_PROFILE_SPINLOCK 0
#endif

template <class BKOFF>
class SpinLockType {
	std::atomic_flag locked = ATOMIC_FLAG_INIT;
	BKOFF bkoff;

#if NUMA_PROFILE_SPINLOCK
	uint64_t counter = 0;

	static size_t rdtsc() {
#if PGASUS_PLATFORM_S390X
		uint64_t tsc;
		__asm__ volatile("stckf %0" : "=Q" (tsc) : : "cc");
		return tsc;
#else
		uint_least32_t hi, lo;
#if PGASUS_PLATFORM_PPC64LE
/// DCL TODO adjust for POWER 64 LE
	FIXME;
#else
		__asm__ volatile("rdtsc": "=a"(lo), "=d"(hi));
#endif
		return (size_t)lo | ((size_t)hi << 32);
#endif
	}
#endif

public:
	SpinLockType() { }

	void lock() {
#if NUMA_PROFILE_SPINLOCK
		size_t t1 = rdtsc();
#endif

		while (locked.test_and_set()) {
			bkoff.backoff();
		}
		bkoff.reset();

#if NUMA_PROFILE_SPINLOCK
		counter += rdtsc() - t1;
#endif
	}
	
	bool try_lock() {
		while (locked.test_and_set()) {
			if (!bkoff.backoff()) return false;
		}
		bkoff.reset();
		return true;
	}
	
	void unlock() {
#if NUMA_PROFILE_SPINLOCK
		size_t t1 = rdtsc();
#endif
		locked.clear();
#if NUMA_PROFILE_SPINLOCK
		counter += rdtsc() - t1;
#endif
	}

	inline size_t count() const {
#if NUMA_PROFILE_SPINLOCK
		return counter;
#else
		return 0;
#endif
	}
};

using SpinLock = SpinLockType<ExponentialBackOff<16,1<<10>>;

}
