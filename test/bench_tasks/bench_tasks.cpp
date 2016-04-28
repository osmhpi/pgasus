#include <cstdio>
#include <cassert>
#include "tasking/tasking.hpp"
#include "timer.hpp"
#include <unistd.h>

using numa::TaskRef;
using numa::TriggerableRef;

void usage(const char *name)
{
	printf("Usage: %s [msTime = 1000] [quiet]\n", name);
	exit(0);
}

static std::atomic<size_t> done(0);
static std::atomic<size_t> total(0);

static std::mutex lock;
static std::vector<int> times;
static Timer<int> timer;

void taskfun(size_t id, size_t left, size_t count)
{
	if (left == 0) {
		if (done.load() == 0)
			left = 1000;
	}

	if (left > 0) {
		numa::async<void>([=]() {
			taskfun(id, left-1, count+1);
		}, 0, numa::Node::curr());
	} else {
		std::lock_guard<std::mutex> guard(lock);
		times.push_back(timer.get_elapsed());
		total += count;
	}
}

int main (int argc, char const* argv[])
{
	if (argc < 1) usage(argv[0]);

	bool quiet = (argc > 2 && !strcmp(argv[2], "quiet"));

	size_t tMs = (argc > 1) ? atoi(argv[1]) : 1000;
	assert(tMs > 0);

	std::atomic<size_t> ids(0);

	timer.start();
	numa::forEachThread(numa::NodeList::allNodes(), [&]() {
		taskfun(ids.fetch_add(1), 100, 0);
	}, 0);

	assert(usleep(tMs * 1000) == 0);
	if (!quiet) printf("[main] done. waiting\n");
	done.store(1);

	// wait a while
	assert(usleep(1000 * 1000) == 0);
	if (!quiet) printf("[main] exiting\n");

	int totalTime = 0;
	for (int t : times) totalTime += t;

	float mtasksPerSec = total.load() * ids.load() / ((float)totalTime * 1000.f);

	if (!quiet) printf("totalTime=%d\n", totalTime);
	if (!quiet) printf("totalTasks=%zd\n", total.load());
	if (!quiet) printf("threads=%zd\n", ids.load());
	if (!quiet) printf("tasks/sec=%8.2f\n", mtasksPerSec);

	if (quiet) printf("%8.2f\n", mtasksPerSec);

	return 0;
}
