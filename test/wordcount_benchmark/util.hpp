#pragma once

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "buffer.hpp"

#define WORDCOUNT_USE_WORD_MAP 0

#if WORDCOUNT_USE_WORD_MAP
#include <unordered_map>
#endif

using WordCount = std::map<std::string, size_t>;

struct TextFile {
public:
	using WordList = std::vector<std::string>;

	explicit TextFile(const std::string &fname);
	~TextFile();

    std::unique_ptr<WordCount> countWords() const;
	size_t count(const std::string &word) const;

#if WORDCOUNT_USE_WORD_MAP
    std::unordered_map<size_t, WordList> lines;
#else
	std::vector<WordList> lines;
#endif
    std::string fileName;
    std::unique_ptr<Buffer> fileData;
	size_t totalWordCount;

private:
	static WordList wordsFromLine(const std::string &line);
	void doExtractLines(Buffer &buf);
};

// returns file data of all files listed in given file
std::vector<std::string> loadFiles(const std::string &fname);

//void addWordMap(WordCount &dst, const WordCount &src);
//std::vector<Word>* sortedWords(const WordCount &wc);

//std::vector<std::vector<std::pair<size_t,size_t>>> prefixSumIterations(size_t elems);
