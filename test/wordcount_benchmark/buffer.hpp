#pragma once

#include <cstddef>
#include <string>

/**
 * Holds binary data
 */
class Buffer {
private:
	char *_data;
	std::string _name;
	size_t _size;
	Buffer() : _data(0), _size(0) {}

public:
	char *data() { return _data; }
	size_t size() { return _size; }
	std::string name() { return _name; }

	static Buffer *fromFile(const char *fname);
	static Buffer *unzip(Buffer *other);
	~Buffer();
};
