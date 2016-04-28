#pragma once

namespace numa {
namespace util {

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


}
}
