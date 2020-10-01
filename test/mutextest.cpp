#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>
#include <sstream>

#include <vector>
#include <list>
#include <iostream>

#include "PGASUS/tasking/tasking.hpp"
#include "PGASUS/mutex.hpp"

#include "test_helper.h"
#include "timer.hpp"

using numa::TaskRef;
using numa::TriggerableRef;


static std::atomic_flag flag;

float tediousCalc(int count) {
	ASSERT_TRUE(!flag.test_and_set());
	
	float val = 0.f;
	for (int i = 0; i < count * 1000; i++)
		val += sin(i) * cos(i);
	
	flag.clear();
	
	return val;
}


template <class Mutex>
void do_test(int tasks, bool dolock) {
	std::list<TriggerableRef> waitTasks;
	
	Mutex mutex;
	
	// IF I REMOVE THE "for (i = 0..tasks-1)", THERE IS NO FUCKING GODDAMN
	// SEG-FUCKING-FAULT! WHAT THE ACTUAL FUCK!?!? 
	// at least not within this method.
	
	for (int i = 0; i < tasks; i++) {
		waitTasks.push_back(numa::async<void>( [=,&mutex] () {
			//printf("Start Task");
			if (dolock) {
				//printf("Lock Task");
				std::lock_guard<Mutex> lock(mutex);
			}
			//printf("Done Task");
		}, 0));
	}
	
	numa::wait(waitTasks);
}


int main (int argc, char const* argv[])
{
	if (argc < 2) {
		printf("Usage: %s taskcount [lock]\n", argv[0]);
		return 0;
	}

	testing::initialize();

	// get task count
	int taskcount = atoi(argv[1]);
	bool lock = (argc > 2 && !strcmp(argv[2], "lock"));
	
	printf("Start SpinLock test\n");
	do_test<numa::SpinLock>(taskcount, lock);
	printf("Done SpinLock test\n");
	
	printf("Start Mutex test\n");
	do_test<numa::Mutex>(taskcount, lock);
	printf("Done Mutex test\n");
	
	return 0;
}
