#pragma once

#include <string>

#define NUMA_ENABLE_DEBUG 1

namespace numa {
namespace debug {

enum DebugLevel {
	INFO = 1,
	DEBUG = 2,
	CRITICAL = 3,
	NONE = 4
};

#if NUMA_ENABLE_DEBUG
void log(DebugLevel lvl, const char *fmt, ...);
void log_id(DebugLevel lvl, int cpuid, const char *fmt, ...);
#else
static inline void log(DebugLevel lvl, const char *fmt, ...) {}
static inline void log_id(DebugLevel lvl, int id, const char *fmt, ...) {}
#endif

}
}
