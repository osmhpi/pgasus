#pragma once

#include "PGASUS/PGASUS-config.h"

namespace numa {
namespace util {

#if PGASUS_PLATFORM_PPC64LE
static_assert(false,
	"No useful rdtsc equivalent available on the current platform.");
#else

typedef uint_fast64_t TscTime;

static inline TscTime rdtsc() {
#if PGASUS_PLATFORM_S390X
	uint64_t tsc;
	__asm__ volatile("stckf %0" : "=Q" (tsc) : : "cc");
	return tsc;
#else
	uint_least32_t hi, lo;
	__asm__ volatile("rdtsc": "=a"(lo), "=d"(hi));
	return (TscTime)lo | ((TscTime)hi << 32);
#endif
}

template <class T>
struct TscCounter
{
private:
	T &dst;
	TscTime start;
public:
	explicit inline TscCounter(T &d)
		: dst(d), start(rdtsc())
	{
	}
	
	inline ~TscCounter() {
		dst += (T)(rdtsc() - start);
	}
};


#endif

}
}
