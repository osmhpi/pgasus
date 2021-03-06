/*	Copyright (c) 2013, Robert Wang, email: robertwgh (at) gmail.com
	All rights reserved. https://sourceforge.net/p/ezsift

	Part of "timer.h" code referred to code here: https://sites.google.com/site/5kk73gpu2011/

	Revision history:
		September, 15, 2013: initial version.
*/

#ifndef _TIMER_H_
#define _TIMER_H_

#include <time.h>

#ifdef _WIN32
#include <windows.h>
#if !defined(_WINSOCK2API_) && !defined(_WINSOCKAPI_)
struct timeval {
	long tv_sec;
	long tv_usec;
};
#endif
#else//_WIN32
#include <sys/time.h>
#endif//_WIN32


template <typename timer_dt>
class Timer
{
public:
	explicit Timer(bool start = false);
	~Timer() {};

	void start();
	void stop();
	timer_dt get_elapsed() const;
	timer_dt get_time();
	timer_dt stop_get();
	timer_dt stop_get_start();

#ifdef _WIN32
	double freq;
	LARGE_INTEGER start_time;
	LARGE_INTEGER finish_time;
#else//_WIN32
	struct timeval start_time;
	struct timeval finish_time;
#endif//_WIN32
};


// Definition
#ifdef _WIN32
int gettimeofday(struct timeval* tv, int t) {
	union {
		long long ns100;
		FILETIME ft;
	} now;

	GetSystemTimeAsFileTime(&now.ft);
	tv->tv_usec = (long) ((now.ns100 / 10LL) % 1000000LL);
	tv->tv_sec = (long) ((now.ns100 - 116444736000000000LL) / 10000000LL);
	return (0);
}// gettimeofday()
#endif//_WIN32

template <typename timer_dt>
Timer<timer_dt>::Timer(bool start)
{
#ifdef _WIN32
	LARGE_INTEGER tmp;
	QueryPerformanceFrequency((LARGE_INTEGER*)&tmp);
	freq = (double)tmp.QuadPart/1000.0;
#endif
	if (start)
		this->start();
}

template <typename timer_dt>
void Timer<timer_dt>::start()
{
#ifdef _WIN32
	QueryPerformanceCounter((LARGE_INTEGER*) &start_time);
#else//_WIN32
	gettimeofday(&start_time, 0);
#endif//_WIN32
}

template <typename timer_dt>
void Timer<timer_dt>::stop()
{
#ifdef _WIN32
	QueryPerformanceCounter((LARGE_INTEGER*) &finish_time);
#else//_WIN32
	gettimeofday(&finish_time, 0);
#endif//_WIN32
}

template <typename timer_dt>
timer_dt Timer<timer_dt>::get_time()
{
#ifdef _WIN32
	const timer_dt interval = (timer_dt)((double)(finish_time.QuadPart
		- start_time.QuadPart)	/ freq);
#else
	// time difference in milli-seconds
	const timer_dt interval = (timer_dt) (1000.0 * ( finish_time.tv_sec - start_time.tv_sec)
		+(0.001 * (finish_time.tv_usec - start_time.tv_usec)));
#endif//_WIN32

	return interval;
}

template <typename timer_dt>
timer_dt Timer<timer_dt>::get_elapsed() const
{
	#ifdef _WIN32
		LARGE_INTEGER curr_time;
		QueryPerformanceCounter((LARGE_INTEGER*) &finish_time);

		const timer_dt interval = (timer_dt)((double)(curr_time.QuadPart - start_time.QuadPart)	/ freq);
	#else//_WIN32
		struct timeval curr_time;
		gettimeofday(&curr_time, 0);
		const timer_dt interval = (timer_dt) (1000.0 * ( curr_time.tv_sec - start_time.tv_sec)
			+(0.001 * (curr_time.tv_usec - start_time.tv_usec)));
	#endif//_WIN32

	return interval;
}

template <typename timer_dt>
timer_dt Timer<timer_dt>::stop_get()
{
	timer_dt interval;
	stop();
	interval = get_time();

	return interval;
}

// Stop the timer, get the time interval, then start the timer again.
template <typename timer_dt>
timer_dt Timer<timer_dt>::stop_get_start()
{
	timer_dt interval;
	stop();
	interval = get_time();
	start();

	return interval;
}


#endif//_TIMER_H_
