#pragma once

/*

NUMA-distributed Hash-Table

	- should be low overhead to compute nodeof(Key)
	- key must not be present within table to be able to predict location
	(could introduce dummy NULL-value though, indicating a possible entry)
	
	Template Parameters
		- class K, class V
		- class HashFunction<K> = std::hash<K>	// returns int64
		- number of elements in each bin (=msource) -> resizing
		OR: number of total bins
	
	
	what about:
		- recursive tree of bins. each bin has e.g. 16 sub-bins
		- sub-bins only get created when the top-bin is full
		-> leads to a very shallow hierarchy of bins
		- bin hierarchy configuration may be stored node-replicated for
		  more efficient lookup
*/

/*

SIMPLE NUMA hash-table

	Template Parameters
		- class K, class V
		- class HashFunction<K> = std::hash<K>	// returns int64
		- number of total bins (ideally 2^N)
	
	Distribution
		- each bin is stored on one node
		- each bin will hold approx. the same number of elements
		- on redistribution: some bins are migrated
	
	Balancing
		- based on: CPU-load, memory-load
		- when: Creation, Iteration, Explicit, Insert/Delete (every N ops)
		- using what for calculation: #Bins vs. #ItemsInBins
	
	Parallelism
		- bins are RW-locked when accessed+migrated
	
	Focus:
		- general-purpose
		- ease-of-use
		- scalability
*/

#include <vector>
#include <cassert>
#include <algorithm>

#include "msource/msource_types.hpp"
#include "malloc.hpp"

#include "base/rwlock.hpp"
#include "base/ref_ptr.hpp"

#include "hashtable/containers.hpp"


namespace numa {


template <class Key, class T, size_t BinBits, class Hash = std::hash<Key> >
class HashTable
{
public:
	typedef Key key_type;
	typedef T mapped_type;
	typedef std::pair<const Key, T> value_type;

private:

	static constexpr size_t BinCount = (1 << BinBits);
	static constexpr size_t BinMask = BinCount-1;
	
	typedef uint64_t HashType;
	
	/**
	 * Single Key/Value pair
	 */
	struct Node : public numa::MemSourceReferenced {
		HashType hash;
		value_type data;
		
		template <class... Args>
		Node(HashType h, const Key &k, Args&&... args)
			: hash(h), data(k, T(std::forward<Args>(args)...))
		{
		}
		
		const Key& key() const { return data.first; }
		T& value() { return data.second; }
	};
	
	typedef numa::RefPtr<Node> NodePtr;
	typedef numa::msvector<NodePtr> NodeList;
	typedef typename NodeList::iterator NodeListIterator;
	
	/**
	 * Bucket holding pairs whose keys produce hash collisions modulo some offset
	 */
	struct Bucket : public numa::MemSourceReferenced, public NodeList {
		RWLock lock;
		size_t index;
		std::atomic_size_t generation;
		
		Bucket(const numa::MemSource &ms, size_t idx, size_t bin_generation)
			: NodeList(ms), index(idx), generation(bin_generation << 32)
		{
			static_assert(sizeof(size_t) > 4, "size_t needs to be 64bit");
		}
		
		Bucket(Bucket &&other)
			: NodeList(other), index(other.index), generation(other.generation.load())
		{
		}
	};
	
	typedef numa::RefPtr<Bucket> BucketPtr;
	
	/**
	 * Iterator that iterates through all elements within one Bucket.
	 * Holds ref-pointers to the nodes, so they will not be deleted.
	 * If the bucket changes, the iterator will try to find the next element
	 * with a higher hash value
	 */
	struct BucketIterator {
	private:
		BucketPtr           _bucket;
		size_t              _generation;
		NodePtr             _curr_node;
		NodeListIterator    _index;
		
		inline void updateCurrNode() {
			_curr_node = (_index != _bucket->end()) ? *_index : nullptr;
		}
	public:
		inline BucketIterator() : _bucket(), _curr_node() {}
		inline BucketIterator(BucketPtr bucket) : _bucket(bucket), _curr_node(nullptr)
		{
			auto guard = _bucket->lock.read_guard();
			_generation = _bucket->generation.load();
			_index = _bucket->begin();
			updateCurrNode();
		}
		inline BucketIterator(BucketPtr bucket, NodeListIterator &it) : _bucket(bucket), _index(it)
		{
			auto guard = _bucket->lock.read_guard();
			_generation = _bucket->generation.load();
			updateCurrNode();
		}
		inline value_type& operator*() {
			return _curr_node->data;
		}
		inline value_type& operator->() {
			return _curr_node->data;
		}
		inline bool next() {
			if (is_end()) return false;

			auto guard = _bucket->lock.read_guard();
			
			// no changes to bucket -> just increment bucket iterator
			if (_generation == _bucket->generation.load()) {
				++_index;
			// bucket was changed -> search for next position within
			} else {
				HashType old_hash = _curr_node->hash;
				_index = _bucket->begin();
				while (_index != _bucket->end() && (*_index)->hash < old_hash)
					++_index;
			}
			
			updateCurrNode();
			
			return !is_end();
		}
		inline bool is_end() const {
			return !_curr_node.valid();
		}
	};
	
	/**
	 * Collection of all buckets that make up a Bin
	 */
	struct BucketSpace
		: public numa::MemSourceReferenced
		, public numa::msvector<BucketPtr>
	{
		size_t              _num_buckets;
		size_t              _num_buckets_mask;
		numa::MemSource     _msource;
		
		BucketSpace(const numa::MemSource &ms, size_t sz, size_t gen)
			: numa::msvector<BucketPtr>(ms)
			, _msource(ms)
		{
			// double space
			_num_buckets = sz;
			_num_buckets_mask = _num_buckets - 1;
		
			// assert power of two
			assert((_num_buckets & _num_buckets_mask) == 0);
		
			// reserve space in new container, create new buckets
			numa::msvector<BucketPtr>::reserve(_num_buckets);
			for (size_t i = 0; i < _num_buckets; i++) {
				Bucket *b = ms.template construct<Bucket>(ms, i, gen);
				numa::msvector<BucketPtr>::push_back(b);
			}
		}
		
		/**
		 * Get Bucket index from given hash (computed for a key)
		 */
		inline Bucket* from_hash(HashType h) {
			return (*this)[(h >> BinBits) & _num_buckets_mask].get();
		}

		size_t itemCount() const {
			size_t sz = 0;
			for (size_t i = 0; i < _num_buckets; i++)
				sz += numa::msvector<BucketPtr>::at(i)->size();
			return sz;
		}
	};
	
	typedef numa::RefPtr<BucketSpace> BucketSpacePtr;
	
	/**
	 * Iterator that iterates through all elements within one BucketSpace
	 */
	struct BucketSpaceIterator : public containers::RecursiveIterator<
			BucketSpaceIterator, value_type, size_t, BucketIterator>
	{
		BucketSpacePtr _buckets;

		inline BucketIterator get_sub_iter(size_t idx) {
			BucketPtr ptr = (*_buckets)[idx];
			return BucketIterator(ptr);
		}
		inline BucketSpaceIterator() : _buckets(nullptr) {}
		inline BucketSpaceIterator(BucketSpacePtr bs, size_t start_bucket, size_t end_bucket = (size_t)-1) {
			_buckets = bs;
			this->init(start_bucket, (end_bucket < bs->size()) ? end_bucket : bs->size());
		}
		inline BucketSpaceIterator(BucketSpacePtr bs, Bucket *bucket, NodeListIterator &it) {
			_buckets = bs;
			assert((*_buckets)[bucket->index].get() == bucket);
			this->init_with_iter(bucket->index, _buckets->size(), BucketIterator(bucket, it));
		}
	};

	/**
	 * Contains buckets, is stored on a specific node, is powered by its own
	 * MemSource. Multiple of these constitute a HashTable's node-local part of
	 * its data
	 */
	struct BinData {
		numa::MemSource                 _msource;

		size_t                          _index;
		HashTable                      *_parent;
		
		size_t                          _max_count;	// before resizing
		float                           _max_load_ratio = 3.0f;
		size_t                          _generation = 0;
		
		/* TODO: make sure each bucket sits in its own cacheline */
		BucketSpacePtr                  _buckets;
		
		/* TODO: the following should ideally reside in an own cacheline */
		std::atomic_size_t              _count = {0};
		RWLock                          _rwlock;

		BinData(const numa::MemSource &ms, HashTable *tbl, size_t idx)
			: _msource(ms)
			, _index(idx)
			, _parent(tbl)
			, _buckets(nullptr)
		{
			resize(64);
		}

		~BinData() {
		}

		static BinData* create(HashTable *tbl, size_t idx, numa::Node node) {
			numa::MemSource ms = numa::MemSource::create(node.physicalId(), 1 << 20, "HashTable::BinData");
			return ms.construct<BinData>(ms, tbl, idx);
		}

		BucketSpacePtr bucketSpace() {
			auto guard = _rwlock.read_guard();
			return _buckets;
		}
		
		/**
		 * Construct new Node within given bucket, from key+hash, creating
		 * the value object of type T from the arguments in args.
		 * This may call default, copy, or specific constructors, depending
		 * on Args.
		 */
/*
		template <class... Args>
		Node* add_to_bucket(Bucket &bucket, const Key &key, HashType h, Args&&... args) {
			auto guard = bucket.lock.write_guard();
			
			_count += 1;
			
			// add new node
			Node *new_node = _msource.template construct<Node>(h, key, std::forward<args>...);
			bucket.push_back(new_node);
			bucket.generation += 1;
			
			return new_node;
		}
*/
		template <class... Args>
		Node* add_to_bucket(Bucket &bucket, const Key &key, HashType h, const Args&... args) {
			auto guard = bucket.lock.write_guard();
			
			_count += 1;
			
			// add new node
			Node *new_node = _msource.template construct<Node>(h, key, args...);
			bucket.push_back(new_node);
			bucket.generation += 1;
			
			return new_node;
		}
		
		/**
		 * Allocates sz bins and rehashes all nodes into these new bins
		 */
		void resize(size_t sz) {
			_max_count = (size_t) (sz * _max_load_ratio);
			_generation += 1;

			BucketSpacePtr newbuckets = _msource.template construct<BucketSpace>(_msource, sz, _generation);
			
			// copy bucket data
			if (_buckets.valid()) {
				for (auto &bucket : *_buckets) {
					for (auto &node : *bucket) {
						newbuckets->from_hash(_parent->_hash(node->key()))->push_back(node);
					}
				}
			}
			
			_buckets = newbuckets;
		}
		
		/**
		 * Call resize operation, if the current number of elements exceeds
		 * the max. load factor
		 */
		inline void resize_if_necessary() {
			auto guard = _rwlock.write_guard();
			
			if (_count.load() >= _max_count)
				resize(2 * _buckets->size());
		}
		
		/**
		 * Run a synchronized operation on this bin.
		 *  - read-lock the bin
		 *  - find bucket for given hash value, lock that bucket read or write
		 *  - search for key in bucket. call found() if found, notfound() if not
		 *  - return the return value form found() / notfound()
		 * The class Detail provides details on what to do.
		 */
		template <class Detail>
		typename Detail::result_type
		operate_on_bin(const Detail &d, const Key &key, HashType h) {
			// Check if we should increase this bin's size?
			if (Detail::does_modify && _count.load() >= _max_count) {
				resize_if_necessary();
			}
			
			auto guard = _rwlock.read_guard();
			
			// retrieve bucket
			Bucket &bucket = *_buckets->from_hash(h);
			
			{
				auto bucket_guard = bucket.lock.template guard<Detail::needs_write_access>();
			
				// check for key in bucket
				for (auto it = bucket.begin(); it != bucket.end(); ++it) {
					if (((*it)->hash == h) && ((*it)->key() == key))
						return d.found(it, this, bucket, key);
				}
			}
			
			return d.notfound(this, bucket, key, h);
		}
		
		/** CRTP base class for bin operations */
		template <class ResultType, bool DoesModify, bool NeedsWriteAccess>
		struct OperationBase {
			typedef ResultType result_type;
			constexpr static bool does_modify = DoesModify;
			constexpr static bool needs_write_access = NeedsWriteAccess;
		};

		/** Return value for key, or insert default-constructed value */
		struct GetOrCreate : public OperationBase<T&, true, false> {
			inline T& found(NodeListIterator &it, BinData*, Bucket&, const Key&) const {
				return (*it)->value();
			}
			inline T& notfound(BinData *bd, Bucket &bucket, const Key &key, HashType h) const {
				return bd->add_to_bucket(bucket, key, h)->value();
			}
		};

		/** Return value for key, or default-constructed value */
		struct Get : public OperationBase<bool, false, false> {
			T *&ret;
			Get(T *&t) : ret(t) {}

			inline bool found(NodeListIterator &it, BinData*, Bucket&, const Key&) const {
				ret = &((*it)->value());
				return true;
			}
			inline bool notfound(BinData*, Bucket&, const Key&, HashType) const {
				return false;
			}
		};
		
		/** Map key to given value */
		template <class Value>
		struct SetValue : public OperationBase<T&, true, true> {
			Value value;
			SetValue(Value v) : value(v) {}
			
			inline T& found(NodeListIterator &it, BinData*, Bucket&, const Key&) const {
				return ((*it)->value = value);
			}
			inline T& notfound(BinData *bd, Bucket &bucket, const Key &key, HashType h) const {
				return bd->add_to_bucket(bucket, key, h, value)->value;
			}
		};
		
		/** Remove key/value pair, if existant */
		struct Remove : public OperationBase<bool, false, true> {
			inline bool found(NodeListIterator &it, BinData *bd, Bucket &bucket, const Key&) const {
				bucket.erase(it);
				bucket.generation += 1;
				bd->_count -= 1;
				return true;
			}
			inline bool notfound(BinData *bd, Bucket &bucket, const Key &key, HashType) const {
				return false;
			}
		};
		
		/** Find iterator, if key/value pair existant */
		struct Find : public OperationBase<BucketSpaceIterator, false, false> {
			inline BucketSpaceIterator found(NodeListIterator &it, BinData *bd, Bucket &bucket, const Key&) const {
				return BucketSpaceIterator(bd, &bucket, it);
			}
			inline BucketSpaceIterator notfound(BinData *bd, Bucket &bucket, const Key &key, HashType) const {
				return BucketSpaceIterator(bd, BucketSpaceIterator::END);
			}
		};
		
		inline T& get_or_create(const Key &key, HashType h) {
			return operate_on_bin(GetOrCreate(), key, h);
		}

		inline bool get(const Key &key, HashType h, T *& dst) {
			return operate_on_bin(Get(), key, h);
		}
		
		inline T& set(const Key &key, HashType h, const T &value) {
			return operate_on_bin(SetValue<const T&>(value), key, h);
		}
		
		inline T& set(const Key &key, HashType h, T &&value) {
			return operate_on_bin(SetValue<T&&>(value), key, h);
		}
		
		inline bool remove(const Key &key, HashType h) {
			return operate_on_bin(Remove(), key, h);
		}
		
		inline BucketSpaceIterator find(const Key &key, HashType h) {
			return operate_on_bin(Find(), key, h);
		}
	};
	
	std::array<BinData*,BinCount>       _bins;
	std::array<numa::Node,BinCount>     _bin_nodes;
	
	Hash                                _hash;
	
	inline size_t get_bin_index(const Key &key) const {
		uint64_t h = _hash(key);
		return h & BinMask;
	}
	
public:

	HashTable(const numa::NodeList &nodes) {
		for (size_t idx = 0; idx < BinCount; idx++) {
			int nodeIdx = (idx * nodes.size() / BinCount);
			_bin_nodes[idx] = nodes[nodeIdx];
			_bins[idx] = BinData::create(this, idx, _bin_nodes[idx]);
		}
	}
	
	~HashTable() {
		for (size_t idx = 0; idx < BinCount; idx++) {
			numa::MemSource::destruct(_bins[idx]);
		}
	}

	size_t size() const {
		size_t sz = 0;
		for (auto bd : _bins)
			sz += bd->_count.load();
		return sz;
	}

	numa::NodeList nodes() const {
		numa::NodeList result;
		for (auto n : _bin_nodes) result.push_back(n);
		auto last = std::unique(result.begin(), result.end());
		result.erase(last, result.end());
		return result;
	}
	
	numa::Place where(const Key &key) const {
		return numa::Place(_bins[get_bin_index(key)]->_msource);
	}
	
	T& operator[](const Key &key) {
		HashType h = _hash(key);
		BinData *bin = _bins[h & BinMask];
		return bin->get_or_create(key, h);
	}

	bool lookup(const Key &key, T*& dst) {
		HashType h = _hash(key);
		return _bins[h & BinMask]->get(key, h, dst);
	}
	
	bool remove(const Key &key) {
		HashType h = _hash(key);
		BinData *bin = _bins[h & BinMask];
		return bin->remove(key, h);
	}
	
	/**
	 * Guaranteed to be executed within correct allocation-context 
	 */
	TaskRef<T*> insertAsync(const Key &key, const std::function<T()> &generator) {
		HashType h = _hash(key);
		size_t bin_index = h & BinMask;
		numa::Node node = _bin_nodes[bin_index];
		BinData *bin = _bins[bin_index];

		return numa::async<T*>([key, generator, bin, h]() {
			numa::Place guard(bin->_msource);
			T *ref = &bin->get_or_create(key, h);
			*ref = generator();
			return ref;
		}, 0, node);
	}
	

	struct Iterator : public containers::RecursiveIterator<
			Iterator, value_type, size_t, BucketSpaceIterator>
	{
		HashTable *_table;

		inline BucketSpaceIterator get_sub_iter(size_t idx) {
			return BucketSpaceIterator(_table->_bins[idx]->bucketSpace(), 0);
		}
		inline Iterator(HashTable *table, size_t idx) {
			_table = table;
			this->init(idx, BinCount);
		}
	};
	
	Iterator begin() {
		return Iterator(this, 0);
	}
	
	Iterator end() {
		return Iterator(this, BinCount);
	}

	class ParallelIteration {
	private:
		std::atomic_size_t _counter;
		std::vector<BucketSpaceIterator> _iters;
	public:
		typedef BucketSpaceIterator Iterator;

		ParallelIteration(HashTable &table, const numa::Node &node, size_t min_units)
			: _counter(0)
		{
			std::vector<BinData*> bins;
			for (size_t i = 0; i < BinCount; i++) {
				if (table._bin_nodes[i] == node)
					bins.push_back(table._bins[i]);
			}

			size_t min_bin_units = (bins.size() + min_units - 1) / bins.size();
			for (BinData *bin : bins) {
				BucketSpacePtr buckets = bin->bucketSpace();
				size_t units = std::min(min_bin_units, buckets->size());
				for (size_t i = 0; i < units; i++) {
					size_t start = (i * buckets->size()) / units;
					size_t end = ((i+1) * buckets->size()) / units;
					_iters.push_back(BucketSpaceIterator(buckets, start, end));
				}
			}
		}

		ParallelIteration(ParallelIteration &&other) : _counter(0), _iters(other._iters) {}
		ParallelIteration(const ParallelIteration &other) : _counter(0), _iters(other._iters) {}

		bool get(BucketSpaceIterator &dst, numa::MemSource &msource) {
			size_t next = _counter.fetch_add(1);
			if (next < _iters.size()) {
				dst = _iters[next];
				msource = dst._buckets->_msource;
				return true;
			}
			return false;
		}
	};

	inline ParallelIteration* iterate(const numa::Node &node, int min_units) {
		if (node.valid()) {
			numa::PlaceGuard guard(node);
			return new ParallelIteration(*this, node, min_units);
		}
		return nullptr;
	}
};

}

