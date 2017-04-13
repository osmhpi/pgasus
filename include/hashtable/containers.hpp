#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <tuple>
#include <type_traits>
#include <utility>
#include <tuple>

#include "msource/node_replicated.hpp"
#include "tasking/tasking.hpp"

namespace numa {
namespace containers {

template <class Impl, class Value, class Index, class SubIter>
class RecursiveIterator
{
private:
	Index       _curr_index;
	Index       _end_index;
	SubIter     _sub_iter;

	inline bool find_next() {
		while (_curr_index < _end_index) {
			_sub_iter = static_cast<Impl*>(this)->get_sub_iter(_curr_index);
			if (!_sub_iter.is_end()) return true;
			_curr_index++;
		}
		return false;
	}

protected:

	RecursiveIterator() {
	}

	inline void init(Index start, Index end) {
		_curr_index = start;
		_end_index = end;
		find_next();
	}

	inline void init_with_iter(Index start, Index end, const SubIter &iter) {
		_curr_index = start;
		_end_index = end;
		_sub_iter = iter;
	}

public:

	inline Value& operator*() {
		return *_sub_iter;
	}

	inline Value* operator->() {
		return &(*_sub_iter);
	}

	inline bool is_end() const {
		return _curr_index >= _end_index;
	}

	inline bool next() {
		if (is_end()) return false;
		if (_sub_iter.next()) return true;
		_curr_index++;
		return find_next();
	}
};

namespace detail {
	template <class Args, class ValueType> struct DistIterTypes {
		typedef typename std::function<void(ValueType&, Args&)> TaskFunction;
		typedef typename std::vector<Args*> ResultType;
		static inline void call(const TaskFunction &fun, ValueType &vt, Args *arg) {
			fun(vt, *arg);
		}
		static inline Args* create() { return new Args(); }
	};
	template <class ValueType> struct DistIterTypes<void,ValueType> {
		typedef typename std::function<void(ValueType&)> TaskFunction;
		typedef void* ResultType;
		static inline void call(const TaskFunction &fun, ValueType &vt, void *arg) {
			fun(vt);
		}
		static inline void* create() { return nullptr; }
	};
}

template <class Iterable, class Arg = void>
class DistributedIteration
{
public:
	typedef typename Iterable::value_type value_type;
	typedef typename detail::DistIterTypes<Arg,value_type>::TaskFunction TaskFunction;

private:
	typedef typename Iterable::ParallelIteration ParallelIteration;
	typedef typename ParallelIteration::Iterator Iterator;

	std::vector<ParallelIteration*>        _iterations;
	ParallelIteration                     *_global_iteration;
	NodeList                               _nodes;

	static inline void iterate(ParallelIteration *piter, const TaskFunction &fun, Arg* arg) {
		if (piter == nullptr) return;

		Iterator it;
		MemSource ms;
		while (piter->get(it, ms)) {
			numa::PlaceGuard guard(ms);
			for (; !it.is_end(); it.next()) {
				value_type &elem = *it;
				detail::DistIterTypes<Arg,value_type>::call(fun, elem, arg);
			}
		}
	}

public:
	DistributedIteration(Iterable &iterable, int min_units) {
		_nodes = iterable.nodes();
		size_t min_node_units = (_nodes.size() + min_units) / _nodes.size();
		_iterations.resize(numa::NodeList::allNodesCount(), nullptr);

		for (numa::Node node : numa::NodeList::allNodes()) {
			_iterations.at(node.logicalId()) = iterable.iterate(node, min_node_units);
		}
		_global_iteration = iterable.iterate(Node(), min_node_units);
	}

	~DistributedIteration() {
		for (ParallelIteration* iter : _iterations) {
			delete iter;
		}
		delete _global_iteration;
	}

	inline std::vector<Arg*> operator()(const TaskFunction &fun, Priority prio) {
		std::vector<Arg*> ret;
		std::mutex mutex;

		numa::wait(numa::forEachThread(_nodes, [this,&fun,&ret,&mutex]() {
			auto threadData = detail::DistIterTypes<Arg,value_type>::create();

			Node node = Node::curr();
			iterate(_iterations.at(node.logicalId()), fun, threadData);

			for (const Node& neighbor : node.nearestNeighbors()) {
				iterate(_iterations.at(neighbor.logicalId()), fun, threadData);
			}

			iterate(_global_iteration, fun, threadData);

			std::lock_guard<std::mutex> lock(mutex);
			ret.push_back(threadData);
		}, prio));

		return ret;
	}
};


template <class ArgType, class Iterable>
inline std::vector<ArgType*> for_each_distr(
		Iterable &iterable,
		const typename detail::DistIterTypes<ArgType,typename Iterable::value_type>::TaskFunction &fun,
		size_t minUnits,
		Priority prio = Priority())
{
	DistributedIteration<Iterable,ArgType> distrIter(iterable, minUnits);
	return distrIter(fun, prio);
}



/**
 * Adapter std::iterator -> numa::Iterator
 */
template <class StdIterator>
class StdIteratorAdapter
{
public:
	typedef typename StdIterator::value_type value_type;

protected:
	StdIterator _curr;
	StdIterator _end;

public:
	StdIteratorAdapter() {}

	StdIteratorAdapter(const StdIterator &begin, const StdIterator &end)
		: _curr(begin), _end(end)
	{
	}

	inline value_type& operator*() { return _curr.operator*(); }
	inline value_type* operator->() { return _curr.operator->(); }

	inline bool is_end() const {
		return _curr == _end;
	}

	inline bool next() {
		if (is_end()) return false;
		++_curr;
		return true;
	}
};

/**
 * ParallelIteration specialization for std:: container
 * adapters
 */
template <class StdIterator>
class StdParallelIteration{
private:
	std::vector<StdIteratorAdapter<StdIterator>> _iters;
	MemSource msource;
	std::atomic<size_t> currIndex;

public:
	typedef StdIteratorAdapter<StdIterator> Iterator;

	StdParallelIteration(const MemSource &ms) : msource(ms), currIndex(0) {}
	~StdParallelIteration() {}

	void addIteratorPair(const StdIterator &begin, const StdIterator &end, size_t elems) {
//		numa::PlaceGuard guard(msource);
		_iters.push_back(StdIteratorAdapter<StdIterator>(begin, end));
	}

	void finalize(int min_units) {
		// TODO:
		// for random-access iterators: split chunks, if need be
		// for non-random-access iterators: we can only split those by hand
	}

	bool get(Iterator &it, MemSource &ms) {
		size_t idx = currIndex.fetch_add(1);
		if (idx < _iters.size()) {
			it = _iters[idx];
			ms = msource;
			return true;
		}
		return false;
	}
};

/**
 * Adapter to make std:: container classes usable in concurrent
 * iteration (must make sure container is not modified during
 * execution)
 */
template <class ContainerType, class PlacementPolicy>
class StdContainerWrapper {
public:
	typedef typename ContainerType::value_type value_type;
	typedef typename ContainerType::iterator base_iterator_type;

	typedef StdIteratorAdapter<base_iterator_type> Iterator;
	typedef StdParallelIteration<base_iterator_type> ParallelIteration;

protected:
	ContainerType &ref;
	PlacementPolicy policy;
	size_t maxChunk;
	bool created;
	std::mutex mutex;
	NodeList iterationNodes;
	std::map<Node, ParallelIteration*> iters;

	MemSource ms;

public:
	StdContainerWrapper(ContainerType &container, const PlacementPolicy &p = PlacementPolicy())
		: ref(container)
		, policy(p)
		, maxChunk((size_t)-1)
		, created(false)
		, iterationNodes(NodeList::allNodes())
	{
		ms = MemSource::create(0, 10000000, "foobar");
	}

	void setMaxChunkSize(size_t s) { maxChunk = s; }
	size_t maxChunkSize() const { return maxChunk; }

	const NodeList& nodes() const { return iterationNodes; }

	inline ParallelIteration* iterate(const numa::Node &node, int min_units) {
		std::lock_guard<std::mutex> lock(mutex);

		numa::PlaceGuard guard(ms);

		if (!created) {
			created = true;

			// create ParallelIteration objects
			for (Node node : NodeList::allNodes())
				iters[node] = new ParallelIteration(ms);
			iters[Node()] = new ParallelIteration(ms);

			// iterators
			base_iterator_type last = ref.begin(), it = ref.begin(), end = ref.end();
			Node currNode = policy(it);
			size_t currChunk = 0;

			// go through container, extract chunks that belong to same node
			while (it != end) {
				++it;
				++currChunk;
				Node node = policy(it);
				if (node != currNode || it == end || currChunk >= maxChunk) {
					iters[currNode]->addIteratorPair(last, it, currChunk);
					last = it;
					currNode = node;
					currChunk = 0;
				}
			}
		}

		iters[node]->finalize(min_units);
		return iters[node];
	}
};

struct IgnorePlacement {
	template <class iterator_type>
	inline Node operator()(const iterator_type &) { return numa::Node(); }
};

struct MemSourceCreationPlacement {
	template <class iterator_type>
	inline Node operator()(const iterator_type &it) {
		return numa::MemSource::nodeOf((void*) &(*it));
	}
};


template <class ArgType = void, class StdContainer, class PlacementPolicy>
inline std::vector<ArgType*> std_for_each_distr(
		StdContainer &stdContainer,
		const typename detail::DistIterTypes<ArgType,typename StdContainer::value_type>::TaskFunction &fun,
		const PlacementPolicy &pol,
		size_t minUnits = 1,
		Priority prio = Priority())
{
	using Wrapper = StdContainerWrapper<StdContainer, PlacementPolicy>;

	Wrapper wrapper(stdContainer, pol);
	wrapper.setMaxChunkSize(stdContainer.size() / minUnits);

	DistributedIteration<Wrapper,ArgType> distrIter(wrapper, minUnits);

	return distrIter(fun, prio);
}

}
}
