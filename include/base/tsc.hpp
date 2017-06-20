#pragma once

#include "PGASUS-config.h"

namespace numa {
namespace util {

#if PGASUS_PLATFORM_PPC64LE
static_assert(false,
	"No useful rdtsc equivalent available on the current platform.");
#else

typedef uint_fast64_t TscTime;

static inline TscTime rdtsc() {
	uint_least32_t hi, lo;
	__asm__ volatile("rdtsc": "=a"(lo), "=d"(hi));
	return (TscTime)lo | ((TscTime)hi << 32);
}

template <class T>
struct TscCounter
{
private:
	T &dst;
	TscTime start;
public:
	inline TscCounter(T &d)
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
