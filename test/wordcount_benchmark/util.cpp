#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cassert>
#include <cstdarg>
#include <cstring>
#include <sstream>
#include <iostream>

#include "PGASUS/msource/msource.hpp"
#include "PGASUS/msource/mmaphelper.h"
#include "timer.hpp"
#include "util.hpp"

TextFile::WordList TextFile::wordsFromLine(const std::string &line) {
	std::string delimiters = " .,!?;:\"'-/()";
	WordList ret;

	bool quit = false;
	std::string::size_type pos, lastPos = 0;

	while (!quit) {
		pos = line.find_first_of(delimiters, lastPos);

		if (pos == std::string::npos) {
			pos = line.length();
			quit = true;
		}

		if (pos != lastPos) {
			std::string word(line.data() + lastPos, pos - lastPos);
			std::transform(word.begin(), word.end(), word.begin(), ::tolower);
			ret.push_back(word);
		}

		lastPos = pos + 1;
	}

	return ret;

}

void TextFile::doExtractLines(Buffer &buf) {
	std::string line;
	std::stringstream sstream;
	sstream.rdbuf()->pubsetbuf(buf.data(), buf.size());
	size_t curr = 0;

	while (std::getline(sstream, line)) {
		if (line.empty()) continue;
		if (line.back() == '\r') line.pop_back();
		lines[curr] = wordsFromLine(line);
		totalWordCount += lines[curr].size();
		curr++;
	}
}

std::vector<std::string> loadFiles(const std::string &fname) {
	std::vector<std::string> ret;

	std::unique_ptr<Buffer> buf = Buffer::fromFile(fname.c_str());
	std::string line;
	std::stringstream sstream;
	sstream.rdbuf()->pubsetbuf(buf->data(), buf->size());

	while (std::getline(sstream, line)) {
		if (line.back() == '\r') line.pop_back();
		ret.push_back(line);
	}

	return ret;
}

TextFile::TextFile(const std::string &fname) {
	fileName = fname;
	fileData = Buffer::fromFile(fname.c_str());
	totalWordCount = 0;

	//zipped?
	if ((fileData->name().substr(fileData->name().size()-3) == ".gz")) {
		fileData = Buffer::unzip(*fileData);
	}
	doExtractLines(*fileData);
}

TextFile::~TextFile() = default;

 std::unique_ptr<WordCount> TextFile::countWords() const {
	std::string delimiters = " .,!?;:\"'-/()";

	std::unique_ptr<WordCount> wc = std::unique_ptr<WordCount>(new WordCount);

	for (auto it = lines.begin(); it != lines.end(); ++it) {
		for (const auto &w : it->second) {
			(*wc)[w] += 1;
		}
	}

	return wc;
}

size_t TextFile::count(const std::string &word) const {
	std::string delimiters = " .,!?;:\"'-/()";
	size_t c = 0;
	std::string lowerword(word);
	std::transform(word.begin(), word.end(), lowerword.begin(), ::tolower);

	for (auto it = lines.begin(); it != lines.end(); ++it) {
		for (const auto &w : it->second) {
			if (w == lowerword)
				c++;
		}
	}

	return c;

}

//void addWordMap(WordCount &dst, const WordCount &src) {
//	for (auto it = src.begin(); it != src.end(); ++it)
//		dst[it->first] += it->second;
//}

//std::vector<std::vector<std::pair<size_t,size_t>>> prefixSumIterations(size_t elems) {
//	std::vector<std::vector<std::pair<size_t,size_t>>> result;

//	for (size_t delta = 1; delta < elems; delta *= 2) {
//		result.resize(result.size() + 1);
//		for (size_t start = 0; start < elems; start += 2*delta) {
//			if (start + delta < elems) {
//				result.back().push_back(std::pair<size_t,size_t>(start, start+delta));
//			}
//		}
//	}

//	return result;
//}
