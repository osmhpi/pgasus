#pragma once

#include <mutex>
#include <vector>
#include <boost/context/all.hpp>

#include "msource/msource_types.hpp"
#include "base/spinlock.hpp"

namespace numa {
namespace tasking {

// start function for a new context
typedef void (*ContextFunction)(intptr_t);

/**
 * Encapsules stack memory and a an fcontext_t
 */
class Context
{
private:
	MemSource                       _msource;
	size_t                          _size;
	void                           *_stack;
	boost::context::fcontext_t      _context;

public:
	Context(ContextFunction fun, size_t size = 81920, const MemSource &ms = MemSource())
	{
		_msource = ms.valid() ? ms : MemSource::global();
		_size = size;
		_stack = _msource.alloc(size);
		reset(fun);
	}
	
	~Context() {
		MemSource::free(_stack);
	}
	
	inline void reset(ContextFunction fun) {
		_context = boost::context::make_fcontext((char*)_stack+_size, _size, fun);
	}
	
	inline boost::context::fcontext_t ctx() {
		return _context;
	}
	
	template <class P>
	void* jump_to(Context *dest, P* p) {
		return (void*)boost::context::jump_fcontext(&_context, dest->_context, (intptr_t)p);
	}
	
	template <class P>
	void* jump_to(boost::context::fcontext_t *dest, P* p) {
		return (void*)boost::context::jump_fcontext(&_context, *dest, (intptr_t)p);
	}
	
	inline intptr_t jump_from(boost::context::fcontext_t *src, intptr_t p) {
		return boost::context::jump_fcontext(src, _context, p);
	}
};

/**
 * Caches contexts for later re-use
 */
class ContextCache
{
private:
	typedef SpinLock Lock;
	typedef msvector<Context*> Storage;
	
	MemSource                   _msource;
	Lock                        _lock;
	Storage                     _data;
	
public:
	ContextCache(const MemSource &ms)
		: _msource(ms)
		, _data(ms)
	{
	}
	
	~ContextCache() {
		for (Context *ctx : _data) {
			ctx->~Context();
			MemSource::free(ctx);
		}
	}
	
	Context* get(ContextFunction fun) {
		Context *result = nullptr;
		{
			std::lock_guard<Lock> lock(_lock);
			if (_data.size() > 0) {
				result = _data.back();
				_data.pop_back();
			}
		}
		if (result == nullptr) {
			void *mem = _msource.alloc(sizeof(Context));
			result = new (mem) Context(fun, 81920, _msource);
		}
		return result;
	}
	
	void store(Context *ctx) {
		std::lock_guard<Lock> lock(_lock);
		_data.push_back(ctx);
	}
};


}
}
