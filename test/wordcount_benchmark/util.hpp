#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>
#include <list>

#include "buffer.hpp"

typedef std::map<std::string, size_t> WordCount;
typedef std::pair<std::string, size_t> Word;

struct TextFile {
public:
	using WordList = std::vector<std::string>;

	TextFile(const std::string &fname);
	~TextFile();

    WordCount *countWords() const;
	size_t count(const std::string &word) const;

	std::unordered_map<size_t, WordList> lines;
    std::string fileName;
    Buffer *fileData;
	size_t totalWordCount;

private:
	static WordList wordsFromLine(const std::string &line);
	void doExtractLines(Buffer *buf);
};

// returns file data of all files listed in given file
std::vector<std::string> loadFiles(const std::string &fname);

//void addWordMap(WordCount &dst, const WordCount &src);
//std::vector<Word>* sortedWords(const WordCount &wc);

//std::vector<std::vector<std::pair<size_t,size_t>>> prefixSumIterations(size_t elems);
