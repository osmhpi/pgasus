#include <algorithm>
#include <memory>
#include <unordered_map>
#include "exec.hpp"

class OpenMPExecutor : public Executor {
	// data storage
	using FileCache = std::unordered_map<std::string, std::unique_ptr<TextFile>>;
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

	virtual void topWords(std::vector<std::string> fileNames) {
		#pragma omp parallel for
		for (size_t i = 0; i < fileNames.size(); i++) {
			/*WordCount *wc =*/ files[fileNames[i]]->countWords();
		}
	}

	virtual size_t countWords(const std::string &w) {
		std::vector<TextFile*> af = allFiles();
		size_t count = 0;

		#pragma omp parallel for reduction(+:count)
		for (size_t i = 0; i < af.size(); i++) {
			count += af[i]->count(w);
		}

		return count;
	}
};

std::unique_ptr<Executor> createExecutor() {
	return std::unique_ptr<Executor>(new OpenMPExecutor());
}
