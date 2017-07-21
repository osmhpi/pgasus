#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>
#include <sstream>

#include <vector>
#include <list>
#include <iostream>

#include "PGASUS/tasking/tasking.hpp"
#include "timer.hpp"
#include "test_helper.h"


using numa::TaskRef;
using numa::TriggerableRef;

float tediousCalc() {
	float val = 0.f;
//	for (int i = 0; i < 7000000; i++)
//		val += sin(i) * cos(i);
	return val;
}


void usage(const char *name) {
	printf("Usage: %s taskcount spawner\n", name);
	exit(0);
}


int main (int argc, char const* argv[])
{
	if (argc < 3) usage(argv[0]);
	
	testing::initialize();

	// get task count
	int count = atoi(argv[1]);
	int count2 = count/2;
	int spawner = atoi(argv[2]);
	
	printf("Main: %d+%d tasks, %d spawner\n",
		count, count2, spawner);
	
	count /= spawner;
	count2 /= spawner;
	
	std::list<TriggerableRef> spawnerTasks;
	for (int i = 0; i < spawner; i++) {
		spawnerTasks.push_back(numa::async<void>( [=] () {
			printf("Spawner[%d] start\n", i);
	
			// run some simple tests
			std::vector<TriggerableRef> tasks;
			std::list<TriggerableRef> waitTasks;
			
			for (int i = 0; i < count; i++) {
				tasks.push_back(numa::async<void>( [i] () {
					Timer<int> t(true);
					tediousCalc();

					// int total = t.stop_get();
//					printf("Task[%d] done: %d.%03ds\n", i, total/1000, total%1000);
				}, 0));
			}
			
			for (int i = 0; i < count2; i++) {
				waitTasks.push_back(numa::async<void>( [=,&tasks] () {
					Timer<int> t(true);
					numa::wait(tasks[2*i].get());
					/*int w1 =*/ t.stop_get_start();
					numa::wait(tasks[2*i+1].get());
					/*int w2 =*/ t.stop_get_start();
					tediousCalc();
					/*int tt =*/ t.stop_get_start();
			
					//printf("Task2[%d] done: w1=%d.%03ds, w2=%d.%03ds t=%d.%03ds\n",
					//	i, w1/1000, w1%1000, w2/1000, w2%1000, tt/1000, tt%1000);
				}, 1));
			}
			
			printf("Spawner[%d] wait\n", i);
		
			numa::wait(waitTasks);
			
			printf("Spawner[%d] done\n", i);
		}, 0));
	}
	
	printf("[Main] wait\n");
	numa::wait(spawnerTasks);
	printf("[Main] done\n");
	
	return 0;
}
