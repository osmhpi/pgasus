#include <cstdio>
#include <cstdarg>

#include "strutil.hpp"

#include <array>
#include <sstream>

namespace numa {
namespace util {

std::string strformat(const std::string * fmt, ...) {
	char buff[4096];
	
	va_list args;
	va_start(args, fmt);
		if (fmt) {
			vsnprintf(buff, sizeof(buff)/sizeof(*buff), fmt->c_str(), args);
		}
	va_end(args);
	
	return std::string(buff);
}

std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}

void toSizeString(size_t sz, char *buf, size_t len)
{
	if (sz < 10000) {
		snprintf(buf, len, "%zd", sz);
		return;
	}

	std::array<const char*, 4> pfs{"k", "m", "g", "t"};
	float v = (float)sz;

	for (const char *pf : pfs) {
		v /= 1024.f;
		if (v < 10.f)        snprintf(buf, len, "%1.3f%s", v, pf);
		else if (v < 100.f)  snprintf(buf, len, "%2.2f%s", v, pf);
		else if (v < 1000.f) snprintf(buf, len, "%3.1f%s", v, pf);
		else continue;
		return;
	}

	snprintf(buf, len, "%zd%s", (size_t)v, pfs.back());
}

std::string toSizeString(size_t sz)
{
	std::array<char,8192> dst;
	toSizeString(sz, dst.data(), dst.size());
	return std::string(dst.data());
}

}
}
