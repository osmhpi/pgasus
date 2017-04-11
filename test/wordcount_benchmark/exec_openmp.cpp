#include <algorithm>
#include <unordered_map>
#include "exec.hpp"

class OpenMPExecutor : public Executor {
	// data storage
	using FileCache = std::unordered_map<std::string, TextFile*>;
	FileCache files;

public:

	virtual std::vector<TextFile*> allFiles() {
		std::vector<TextFile*> f;
		std::transform(files.begin(), files.end(), std::back_inserter(f), [](FileCache::value_type v) { return v.second; });
		return f;
	}

	virtual void loadFiles(const std::vector<std::string> &fileNames) {
		#pragma omp parallel for
		for (size_t i = 0; i < fileNames.size(); i++) {
			TextFile *f = new TextFile(fileNames[i]);

			#pragma omp critical(fileaccess)
			files[fileNames[i]] = f;
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

Executor *createExecutor() {
	return new OpenMPExecutor();
}
