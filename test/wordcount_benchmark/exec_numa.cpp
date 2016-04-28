#include <algorithm>

#include "tasking/tasking.hpp"
#include "hashtable/containers.hpp"
#include "hashtable/hashtable.hpp"
#include "synced_containers.hpp"
#include "malloc.hpp"

#include "exec.hpp"

using numa::TaskRef;
using numa::TriggerableRef;

class NumaExecutor : public Executor {
	// data storage
	using FileCache = numa::HashTable<std::string, TextFile*, 6>;
	FileCache files;

public:

	NumaExecutor() : files(numa::NodeList::allNodes()) {}

	virtual std::vector<TextFile*> allFiles() {
		std::vector<TextFile*> f;
		for (auto it = files.begin(); !it.is_end(); it.next())
			f.push_back(it->second);
		return f;
	}

	virtual void loadFiles(const std::vector<std::string> &fileNames) {
		std::list<TriggerableRef> waitList;

		for (const std::string &file : fileNames) {
			waitList.push_back(files.insertAsync(file, [file]() {
				return new TextFile(file);
			}));
		}

		numa::wait(waitList);
	}

	virtual void topWords(std::vector<std::string> fileNames) {
		std::list<TriggerableRef> waitList;

		for (const std::string &file : fileNames) {
			waitList.push_back(numa::async<void>([this,file]() {
				WordCount *wc = files[file]->countWords();
			}, 0, files.where(file).getNode()));
		}

		numa::wait(waitList);
	}

	virtual size_t countWords(const std::string &w) {
		auto ret = numa::containers::for_each_distr<size_t>(files, [w](FileCache::value_type &value, size_t &acc) {
			numa::PlaceGuard guard(numa::Node::curr());
			acc += value.second->count(w);
		}, files.size());

		return std::accumulate(ret.begin(), ret.end(), 0, [](size_t v, size_t *acc) { return v + *acc; });
	}
};

Executor *createExecutor() {
	return new NumaExecutor();
}
