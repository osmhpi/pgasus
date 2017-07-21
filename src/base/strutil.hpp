#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "PGASUS/base/hpinuma_base_export.h"

namespace numa {
namespace util {

HPINUMA_BASE_EXPORT std::string strformat(const std::string * fmt, ...);

HPINUMA_BASE_EXPORT std::vector<std::string> split(const std::string &s, char delim);

HPINUMA_BASE_EXPORT void toSizeString(size_t sz, char *buf, size_t len);
HPINUMA_BASE_EXPORT std::string toSizeString(size_t sz);
template <size_t N> void toSizeString(size_t sz, std::array<char,N> &dst) {
	toSizeString(sz, dst.data(), dst.size());
}

}
}
