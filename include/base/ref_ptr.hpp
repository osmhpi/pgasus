#pragma once

#include <cassert>
#include <atomic>
#include <functional>

#include "util/hpinuma_util_export.h"

namespace numa {

/** 
 * Reference pointer
 */
template <class T>
class RefPtr {
private:
	T *ptr;
	
	inline void ref() {
		if (ptr != nullptr) ptr->ref();
	}
	
	inline void unref() {
		if (ptr != nullptr) ptr->unref();
	}
	
	template <class U>
	friend class RefPtr;
	
public:
	RefPtr(): ptr(nullptr) {
	}
	RefPtr(T *t) : ptr(t) {
		ref(); 
	}
	
	RefPtr(const RefPtr<T>& r) : ptr(r.ptr) {
		ref(); 
	}
	RefPtr(RefPtr<T> &&r) : ptr(r.ptr) { 
		r.ptr = nullptr; 
	}
	
	template <class S>
	RefPtr(const RefPtr<S>& r) : ptr(dynamic_cast<T*>(r.ptr)) {
		static_assert(std::is_base_of<T,S>::value || std::is_base_of<S,T>::value, 
			"Only create RefPtrs from base/derived classes");
		assert((ptr == nullptr) == (r.ptr == nullptr));
		ref();
	}
	
	template <class S>
	RefPtr(RefPtr<S>&& r) : ptr(dynamic_cast<T*>(r.ptr)) {
		static_assert(std::is_base_of<T,S>::value || std::is_base_of<S,T>::value, 
			"Only create RefPtrs from base/derived classes");
		assert((ptr == nullptr) == (r.ptr == nullptr));
		r.ptr = nullptr;
	}
	
	~RefPtr() { 
		unref(); 
		ptr = nullptr;
	}
	
	inline RefPtr<T>& operator=(const RefPtr<T> &r) { 
		if (r.ptr != ptr) {
			T *old = ptr;
			ptr = r.ptr;
			ref();
			if (old != nullptr) old->unref();
		}
		return *this;
	}

	inline RefPtr<T>& operator=(RefPtr<T> &&r) {
		unref();
		
		ptr = r.ptr;
		r.ptr = nullptr;
		
		return *this;
	}
	
	inline bool operator==(const T *t) const { return ptr==t; }
	inline bool operator!=(const T *t) const { return ptr!=t; }
	inline bool operator==(const RefPtr<T> &t) const { return ptr==t.ptr; }
	inline bool operator!=(const RefPtr<T> &t) const { return ptr!=t.ptr; }
	
	inline T* get() const { return ptr; }
	inline T& operator*() const { return *ptr; }
	inline T* operator->() const { return ptr; }
	
	inline bool valid() const { return ptr != nullptr; }
};


/**
 * Base class for ref-counted objects. Must NOT be allocated on the stack,
 * but using new().
 */
class HPINUMA_UTIL_EXPORT Referenced
{
private:
	std::atomic_int_least32_t   _ref_count;
	
protected:
	friend class numa::RefPtr<Referenced>;
	
	Referenced() : _ref_count(0) {
	}
	
	Referenced(const Referenced&) = delete;
	Referenced(Referenced&&) = delete;
	
	Referenced& operator=(const Referenced&) = delete;
	Referenced& operator=(Referenced&&) = delete;

protected:
	typedef std::function<void(Referenced*&)> DeleteFunctor;
	
	static void libc_delete(Referenced *&r) {
		delete r;
		r = nullptr;
	}
	
	virtual DeleteFunctor deleter() {
		return DeleteFunctor(libc_delete);
	}
	
public:
	
	virtual ~Referenced() {
		assert(_ref_count.load() == 0);
	}
	
	inline void ref() {
		_ref_count.fetch_add(1);
	}
	
	inline void unref() {
		if (_ref_count.fetch_sub(1) == 1) {
			DeleteFunctor del = deleter();
			Referenced *self = this;
			del(self);
		}
	}
	
	inline int_least32_t ref_count() {
		return _ref_count.load();
	}
};

}
