#pragma once

#include <string>

#include "PGASUS/PGASUS-config.h"
#include "PGASUS/base/PGASUS_base_export.h"

namespace numa {
namespace debug {

enum DebugLevel {
	INFO = 1,
	DEBUG = 2,
	CRITICAL = 3,
	NONE = 4
};

#if ENABLE_DEBUG_LOG
void PGASUS_BASE_EXPORT log(DebugLevel lvl, const char *fmt, ...);
void PGASUS_BASE_EXPORT log_id(DebugLevel lvl, int cpuid, const char *fmt, ...);
#else
inline void log(DebugLevel lvl, const char *fmt, ...) {}
inline void log_id(DebugLevel lvl, int id, const char *fmt, ...) {}
#endif

}
}
