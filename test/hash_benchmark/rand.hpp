#pragma once

#include <thread>

template <class NumType>
class FastRand {
private:
	/* These state variables must be initialized so that they are not all zero. */
	NumType x, y, z, w;

public:
	explicit FastRand(NumType seed = std::hash<std::thread::id>()(std::this_thread::get_id()))
		: x(seed), y(0), z(0), w(0)
	{
	}
	NumType operator()() {
		NumType t = x ^ (x << 11);
		x = y; y = z; z = w;
		return w = w ^ (w >> 19) ^ t ^ (t >> 8);
	}
};


