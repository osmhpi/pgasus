#include <algorithm>
#include <numeric>

#include "PGASUS/malloc.hpp"
#include "PGASUS/synced_containers.hpp"
#include "PGASUS/hashtable/hashtable.hpp"
#include "PGASUS/hashtable/containers.hpp"
#include "PGASUS/tasking/tasking.hpp"

#include "exec.hpp"

using numa::TaskRef;
using numa::TriggerableRef;

class NumaExecutor : public Executor {
	// data storage
	using FileCache = numa::HashTable<std::string, std::unique_ptr<TextFile>, 6>;
	FileCache files;

public:

	NumaExecutor() : files(numa::NodeList::logicalNodes()) {}

	virtual std::vector<TextFile*> allFiles() {
		std::vector<TextFile*> f;
		for (auto it = files.begin(); !it.is_end(); it.next())
			f.push_back(it->second.get());
		return f;
	}

	virtual void loadFiles(const std::vector<std::string> &fileNames) {
		std::list<TriggerableRef> waitList;

		for (const std::string &file : fileNames) {
			waitList.push_back(files.insertAsync(file, [file]() {
				return std::unique_ptr<TextFile>(new TextFile(file));
			}));
		}

		numa::wait(waitList);
	}

	virtual void topWords(const std::vector<std::string> &fileNames) {
		std::list<TriggerableRef> waitList;

		for (const std::string &file : fileNames) {
			waitList.push_back(numa::async<void>([this,file]() {
				/*WordCount *wc =*/ files[file]->countWords();
			}, 0, files.where(file).getNode()));
		}

		numa::wait(waitList);
	}

	virtual std::vector<size_t> countWords(const std::vector<std::string> &words) {
		std::vector<size_t> counts(words.size(), 0);
		// TODO parallelize words processing
		for (size_t wi = 0; wi < words.size(); ++wi) {
			const std::string& w = words[wi];
			auto ret = numa::containers::for_each_distr<size_t>(files, [w](FileCache::value_type &value, size_t &acc) {
				numa::PlaceGuard guard(numa::Node::curr());
				acc += value.second->count(w);
			}, files.size());
			const size_t count =
				std::accumulate(ret.begin(), ret.end(), 0, [](size_t v, size_t *acc)
					{ return v + *acc; });
			counts[wi] = count;
		}
		return counts;
	}
};

std::unique_ptr<Executor> createExecutor() {
	return std::unique_ptr<Executor>(new NumaExecutor());
}
