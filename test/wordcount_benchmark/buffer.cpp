#include "buffer.hpp"

#include <zlib.h>
#include <cstdio>
#include <cstring>
#include <cassert>

Buffer *Buffer::fromFile(const char *fname) {
	Buffer *buf = new Buffer();
	FILE *f = fopen(fname, "rb");
	assert(f != nullptr);

	fseek(f, 0, SEEK_END);
	buf->_size = ftell(f);
	fseek(f, 0, SEEK_SET);

	buf->_data = new char[buf->_size];
	assert(fread(buf->_data, 1, buf->_size, f) == buf->_size);
	fclose(f);

	buf->_name = fname;
	return buf;
}

Buffer *Buffer::unzip(Buffer *other) {
	// alloc buffer
	Buffer *buf = new Buffer();
	buf->_size = other->_size * 4;
	buf->_data = (char*) malloc(buf->_size);

	// open stream
	z_stream zs;
	memset(&zs, 0, sizeof(z_stream));
	assert(inflateInit2(&zs, 15|32) == Z_OK);
	zs.next_in = (Bytef*) other->_data;
	zs.avail_in = other->_size;

	// output buffer
	size_t ret, step = 65536, currpos = 0;

	do {
		// make sure buffer is large enough
		while (currpos + step >= buf->_size) {
			buf->_size *= 2;
			buf->_data = (char*) realloc(buf->_data, buf->_size);
		}

		zs.next_out = (Bytef*) (buf->_data + currpos);
		zs.avail_out = step;

		ret = inflate(&zs, 0);

		currpos = zs.total_out;
	} while (ret == Z_OK);

	buf->_size = currpos;

	inflateEnd(&zs);
	assert(ret == Z_STREAM_END);

	buf->_name = other->_name;
	return buf;
}

Buffer::~Buffer() {
	delete []_data;
}
