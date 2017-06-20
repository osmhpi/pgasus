#pragma once

#include <cstddef>
#include <memory>
#include <string>

/**
 * Holds binary data
 */
class Buffer {
private:
	std::unique_ptr<char[]> _data;
	std::string _name;
	size_t _size;
public:
	Buffer();
	~Buffer();

	char *data() { return _data.get(); }
	const char *data() const { return _data.get(); }
	size_t size() const { return _size; }
	const std::string& name() const { return _name; }

	static std::unique_ptr<Buffer> fromFile(const std::string &fname);
	static std::unique_ptr<Buffer> unzip(const Buffer &other);
};
