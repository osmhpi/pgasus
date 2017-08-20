#pragma once

#include <mutex>
#include <cassert>
#include <tuple>

#include "PGASUS/msource/msource_types.hpp"

#include "util/spinlock.hpp"

namespace numa {
namespace msource {


/**
 * Control all global instances of given class type, indexable by int.
 */
template <class T, class LockType = numa::util::SpinLock, class... ArgTypes>
class Singleton
{
	private:
		LockType                _global_lock;
		T*                      _global_data = nullptr;
		std::tuple<ArgTypes...> _args;
		int                     _initialized = 0;
		
		/**
		 * Helper taken from
		 * http://stackoverflow.com/questions/16868129/how-to-store-variadic-template-arguments
		 */
		template <int... Is>
		struct index {};

		template <int N, int... Is>
		struct gen_seq : gen_seq<N - 1, N - 1, Is...> {};

		template <int... Is>
		struct gen_seq<0, Is...> : index<Is...> {};
		
		template <typename... Args, int... Is>
		inline T* create(const std::tuple<ArgTypes...>& tup, index<Is...>) {
			return new (MemSource::global()->alloc(sizeof(T))) T(std::get<Is>(tup)...);
		}
	
	public:
		Singleton(ArgTypes ... args)
			: _args(std::forward<ArgTypes>(args)...)
		{
			_initialized = 1;
		}
		
		Singleton(Singleton &&other)
			: _global_data(other._global_data)
			, _args(other._args)
		{
			other._global_data = nullptr;
		}
		
		~Singleton() {
			std::lock_guard<LockType> lock(_global_lock);
			
			if (_global_data != nullptr) {
				_global_data->~T();
				MemSource::free((void*) _global_data);
			}
		}
		
		Singleton(const Singleton &other) = delete;
		Singleton &operator=(const Singleton &other) = delete;
		
		Singleton &operator=(Singleton &&other) {
			this->_global_data = other._global_data;
			this->_args = other._args;
			other._global_data = nullptr;
		}
		
		T *get() {
			assert(_initialized != 0);
			
			std::lock_guard<LockType> lock(_global_lock);

			if (_global_data == nullptr) {
				auto indices = gen_seq<sizeof...(ArgTypes)>();
				_global_data = create(_args, indices);
			}
			
			return _global_data;
		}
		
		T* operator->() {
			return get();
		}
};

template <class T>
struct SingletonMaker {
	inline Singleton<T,numa::util::SpinLock> operator()() const {
		return Singleton<T,numa::util::SpinLock>(); 
	}
	
	template <class ... Args>
	inline Singleton<T,numa::util::SpinLock,Args...> operator()(Args&&... args) const {
		return Singleton<T,numa::util::SpinLock,Args...>(std::forward<Args>(args)...); 
	}
};

template <class T, class LockType = numa::util::SpinLock, class ... Args>
Singleton<T,LockType,Args...> make_singleton(Args&& ... args) {
	auto maker = SingletonMaker<T>();
	return maker(std::forward<Args>(args)...);
}


}
}
