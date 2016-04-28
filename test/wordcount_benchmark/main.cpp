#include <cstdio>
#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <ctime>
#include <algorithm>
#include <numeric>

#include "util.hpp"
#include "exec.hpp"
#include "timer.hpp"

std::string readline() {
	std::string ret;
	std::getline(std::cin, ret);
	return ret;
}

int main (int argc, char const* argv[])
{
	srand(time(0));

	// check args
	if (argc < 2) {
		fprintf(stderr, "Usage : %s FILES [maxfiles=-1] [top=10] [count=10]\n", argv[0]);
		return 1;
	}
	size_t nTop = 10;
	size_t nCount = 10;
	ssize_t nFiles = -1;

	if (argc > 2) assert(sscanf(argv[2], "%zd", &nFiles) == 1);
	if (argc > 3) assert(sscanf(argv[3], "%zd", &nTop) == 1);
	if (argc > 4) assert(sscanf(argv[4], "%zd", &nCount) == 1);

	Executor *exec = createExecutor();
	Timer<int> timer(true);

	// load files
	std::vector<std::string> fileNames = loadFiles(argv[1]);
	if (nFiles >= 0)
		fileNames.resize(std::min((size_t)nFiles, fileNames.size()));
	exec->loadFiles(fileNames);
	int tLoad = timer.stop_get_start();

	// count lines and words
	size_t totalLines = 0, totalWords = 0;
	std::vector<TextFile*> allFiles = exec->allFiles();
	assert(allFiles.size() == fileNames.size());
	for (TextFile *lf : allFiles) {
		totalLines += lf->lines.size();
		totalWords += lf->totalWordCount;
	}
	printf("%d\ttime(Loaded Files)\n", tLoad);
	printf("%zd\tcount(FilesTotal)\n", allFiles.size());
	printf("%.2f\tcount(M LinesTotal)\n", totalLines / 1000000.f);
	printf("%.2f\tcount(M WordsTotal)\n", totalWords / 1000000.f);
	printf("%.2f\tavg(M WordsLoad / sec)\n", totalWords / (1000.f * tLoad));

	// top
	std::vector<std::string> topFiles;
	size_t topFilesWordCount = 0;
	for (size_t i = 0; i < allFiles.size() * nTop; i++) {
		TextFile *lf = allFiles[rand() % allFiles.size()];
		topFiles.push_back(lf->fileName);
		topFilesWordCount += lf->totalWordCount;
	}
	exec->topWords(topFiles);
	int tTop = timer.stop_get_start();
	printf("%d\ttime(Top)\n", tTop);
	printf("%.2f\tcount(M TopWords)\n", topFilesWordCount / 1000000.f);
	printf("%.2f\tavg(M TopWords / sec)\n", topFilesWordCount / (1000.f * tTop));

	// count
	std::vector<std::string> countWords;
	countWords.resize(nCount, "for");
	for (size_t i = 0; i < countWords.size(); i++) {
		exec->countWords(countWords[i]);
	}
	int tCount = timer.stop_get_start();
	printf("%d\ttime(CountWords)\n", tCount);
	printf("%.2f\tcount(M CountWords)\n", totalWords * countWords.size() / 1000000.f);
	printf("%.2f\tavg(M CountWords / sec)\n", totalWords * countWords.size() / (1000.f * tCount));

	fflush(stdout);

	return 0;
}
