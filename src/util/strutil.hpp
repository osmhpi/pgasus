#pragma once

#include <string>
#include <vector>
#include <array>

namespace numa {
namespace util {

std::string strformat(const std::string &fmt, ...);

std::vector<std::string> split(const std::string &s, char delim);

void toSizeString(size_t sz, char *buf, size_t len);
std::string toSizeString(size_t sz);
template <size_t N> void toSizeString(size_t sz, std::array<char,N> &dst) {
	toSizeString(sz, dst.data(), dst.size());
}

}
}
