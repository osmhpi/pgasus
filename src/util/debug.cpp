#include "util/debug.hpp"

#if ENABLE_DEBUG_LOG

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>

#include "base/node.hpp"
#include "util/timer.hpp"

namespace numa {
namespace debug {

static DebugLevel findOutDebugLevel() {
	DebugLevel lvl = DebugLevel::CRITICAL;

	const char *str = getenv("NUMA_DEBUG");
	if (str != nullptr) {
		if (!strcmp(str, "INFO")) lvl = INFO;
		else if (!strcmp(str, "DEBUG")) lvl = DEBUG;
		else if (!strcmp(str, "CRITICAL")) lvl = CRITICAL;
		else if (!strcmp(str, "NONE")) lvl = NONE;
		else {
			fprintf(stderr, "Invalid NUMA_DEBUG value: %s. Supported are: [INFO, DEBUG, CRITICAL, NONE]\n", str);
		}
	}

	return lvl;
}

static DebugLevel debugLevel() {
	static DebugLevel lvl = findOutDebugLevel();
	return lvl;
}

static Timer<int>& timer() {
	static Timer<int> s_timer(true);
	return s_timer;
}

static bool s_logging_enabled = true;

inline void print(int id, const char *buf) {
	int totalTime = timer().get_elapsed();
	
	FILE *out = stdout;

	if (id >= 0) {
		Node node = Node::forCpuid(id);
	
		fprintf(out, "[%3d.%03d] %2d.%02d: %s\n",
			totalTime/1000, totalTime%1000,
			node.physicalId(), node.indexOfCpuid(id),
			buf);
	} else {
		fprintf(out, "[%3d.%03d] %s\n",
			totalTime/1000, totalTime%1000,
			buf);
	}
	
	fflush(out);
}

void log_id(DebugLevel lvl, int id, const char *fmt, ...)
{
	if (!s_logging_enabled || (int)lvl < (int)debugLevel())
		return;
		
	char buf[4096];

	va_list args;
	va_start(args, fmt);
		vsprintf(buf, fmt, args);
	va_end(args);
	
	print(id, buf);
}

void log(DebugLevel lvl, const char *fmt, ...)
{
	if (!s_logging_enabled || (int)lvl < (int)debugLevel())
		return;
		
	char buf[4096];

	va_list args;
	va_start(args, fmt);
		vsprintf(buf, fmt, args);
	va_end(args);
	
	print(-1, buf);
}

void set_logging_enabled(bool enabled)
{
	s_logging_enabled = enabled;
}

}
}

#endif
