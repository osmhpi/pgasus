#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <omp.h>

#include "exec.hpp"

class OpenMPExecutor : public Executor {
	// data storage
	using FileCache = std::map<std::string, std::unique_ptr<TextFile>>;
	FileCache files;

public:

	virtual std::vector<TextFile*> allFiles() {
		std::vector<TextFile*> f;
		std::transform(files.begin(), files.end(), std::back_inserter(f),
			[](const FileCache::value_type & v) { return v.second.get(); });
		return f;
	}

	virtual void loadFiles(const std::vector<std::string> &fileNames) {
		#pragma omp parallel for
		for (size_t i = 0; i < fileNames.size(); i++) {
			auto f = std::unique_ptr<TextFile>(new TextFile(fileNames[i]));

			#pragma omp critical(fileaccess)
			files[fileNames[i]] = std::move(f);
		}
	}

	virtual void topWords(const std::vector<std::string> &fileNames) {

		struct SmallerOp {
			using Type1 = FileCache::value_type;
			using Type2 = std::string;
			bool operator()(const Type1& lhs, const Type2& rhs) const {
				return lhs.first < rhs;
			}
			bool operator()(const Type2& lhs, const Type1& rhs) const {
				return lhs < rhs.first;
			}
		};

		std::vector<const TextFile*> relevantFiles;
		struct BackInserter {
			BackInserter(std::vector<const TextFile*> & container)
				: container{ container } {}
			BackInserter& operator=(const FileCache::value_type& pair) {
				container.push_back(pair.second.get());
				return *this;
			}
			BackInserter& operator*() { return *this; }
			BackInserter& operator++() { return *this; }
			BackInserter& operator++(int) { return *this; }
			std::vector<const TextFile*> & container;
		};

		std::set_intersection(files.begin(), files.end(),
			fileNames.begin(), fileNames.end(),
			BackInserter(relevantFiles),
			SmallerOp());

		#pragma omp parallel for
		for (size_t i = 0; i < relevantFiles.size(); ++i) {
			relevantFiles[i]->countWords();
		}
	}

	virtual std::vector<size_t> countWords(const std::vector<std::string> &words) {
		std::vector<TextFile*> af = allFiles();
		const size_t numWords = words.size();
		std::vector<size_t> counts(numWords, 0);

		#pragma omp parallel for
		for (size_t	i = 0; i < af.size(); ++i) {
			std::vector<size_t> localCounts(numWords);

			#pragma omp parallel for
			for (size_t wi = 0; wi < numWords; ++wi) {
				localCounts[wi] += af[i]->count(words[wi]);
			}

			#pragma omp critical
			for (size_t wi = 0; wi < numWords; ++wi) {
				counts[wi] += localCounts[wi];
			}
		}

		return counts;
	}
};

std::unique_ptr<Executor> createExecutor() {
	return std::unique_ptr<Executor>(new OpenMPExecutor());
}
