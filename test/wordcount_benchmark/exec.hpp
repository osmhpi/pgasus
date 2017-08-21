#pragma once

#include <memory>
#include <string>
#include <vector>

#include "util.hpp"

class Executor {
public:
	virtual std::vector<TextFile*> allFiles() = 0;
	virtual void loadFiles(const std::vector<std::string> &fileNames) = 0;
	virtual void topWords(const std::vector<std::string> &fileNames) = 0;
	virtual std::vector<size_t> countWords(const std::vector<std::string> &words) = 0;
};

std::unique_ptr<Executor> createExecutor();
