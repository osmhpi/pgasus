#pragma once

#include <memory>
#include <string>
#include <vector>
#include "util.hpp"

class Executor {
public:
	virtual std::vector<TextFile*> allFiles() = 0;
	virtual void loadFiles(const std::vector<std::string> &fileNames) = 0;
	virtual void topWords(std::vector<std::string> fileNames) = 0;
	virtual size_t countWords(const std::string &w) = 0;
};

std::unique_ptr<Executor> createExecutor();
