#include "buffer.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <zlib.h>

#include "test_helper.h"

std::unique_ptr<Buffer> Buffer::fromFile(const std::string &fname) {
	std::unique_ptr<Buffer> buf = std::unique_ptr<Buffer>(new Buffer());
	FILE *f = fopen(fname.c_str(), "rb");
	ASSERT_TRUE(f != nullptr);

	fseek(f, 0, SEEK_END);
	buf->_size = ftell(f);
	fseek(f, 0, SEEK_SET);

	buf->_data = std::unique_ptr<char[]>(new char[buf->_size]);
	ASSERT_TRUE(fread(buf->_data.get(), 1, buf->_size, f) == buf->_size);
	fclose(f);

	buf->_name = fname;
	return buf;
}

std::unique_ptr<Buffer> Buffer::unzip(const Buffer &other) {
	// alloc buffer
	std::unique_ptr<Buffer> buf = std::unique_ptr<Buffer>(new Buffer());
	buf->_size = other._size * 4;
	buf->_data = std::unique_ptr<char[]>(new char[buf->_size]);

	// open stream
	z_stream zs;
	memset(&zs, 0, sizeof(z_stream));
	ASSERT_TRUE(inflateInit2(&zs, 15|32) == Z_OK);
	zs.next_in = (Bytef*) other._data.get();
	zs.avail_in = other._size;

	// output buffer
	size_t ret, step = 65536, currpos = 0;

	do {
		// make sure buffer is large enough
		while (currpos + step >= buf->_size) {
			const size_t newSize = buf->_size * 2u;
			auto newData = std::unique_ptr<char[]>(new char[buf->_size]);
			std::copy_n(buf->_data.get(), buf->_size, newData.get());
			buf->_data = std::move(newData);
			buf->_size = newSize;
		}

		zs.next_out = (Bytef*) (buf->_data.get() + currpos);
		zs.avail_out = step;

		ret = inflate(&zs, 0);

		currpos = zs.total_out;
	} while (ret == Z_OK);

	buf->_size = currpos;

	inflateEnd(&zs);
	ASSERT_TRUE(ret == Z_STREAM_END);

	buf->_name = other._name;
	return buf;
}

Buffer::Buffer() { }

Buffer::~Buffer() = default;
