#include <algorithm>
#include <cassert>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "exec.hpp"
#include "timer.hpp"
#include "util.hpp"

class Args
{
public:
	static Args parse(int argc, char const* argv[]) {
		Args args;
		args.doParse(argc, argv);
		return args;
	}

	std::string exeName;
	std::string benchName;
	std::string fileName;
	size_t maxFiles = -1;
	size_t nTop = -1;
	size_t nCount = 10;
	bool printHeader = false;
	bool printColumns = false;

	void printUsage() {
		fprintf(stderr,
			"Usage : %s FILES [maxfiles=-1] [top=-1] [count=10] [benchName=%s] [-printHeader] [-printColumns]\n"
			"    top=N: Use each loaded file N times in top words.\n"
			"	 count=N: Generate a list of N words for count words.\n",
			exeName.c_str(), benchName.c_str());
	}

private:
	void doParse(int argc, char const* argv[]);
	bool checkArg(const std::string& argv, const std::string& name, std::string& value) {
		if (argv.size() < name.size() + 1
			|| argv.substr(0, name.size()) != name
			|| argv[name.size()] != '=') {
			return false;
		}
		value = argv.substr(name.size() + 1);
		return true;
	}
	template<typename T>
	bool checkArg(const std::string& argv, const std::string& name, T& value);
};

template<>
bool Args::checkArg<int>(const std::string& argv, const std::string& name, int& value) {
	std::string str;
	if (!checkArg(argv, name, str)) {
		return false;
	}
	if (str.empty()) {
		return false;
	}
	try {
		value = std::stoi(str);
		return true;
	} catch(...) {
		printUsage();
		exit(1);
	}
}
template<>
bool Args::checkArg<bool>(const std::string& argv, const std::string& name, bool& value) {
	value = argv == name;
	return value;
}


void Args::doParse(int argc, char const* argv[]) {
	exeName = argv[0];
	const auto nameClampIdx = exeName.find_last_of("/\\");
	benchName = nameClampIdx == std::string::npos
		? exeName
		: exeName.substr(nameClampIdx + 1);
	if (argc < 2) {
		printUsage();
		exit(1);
	}
	fileName = argv[1];
	int intValue;
	bool boolValue;
	std::string stringValue;
	for (int i = 2; i < argc; ++i) {
		if (checkArg(argv[i], "maxfiles", intValue)) {
			maxFiles = intValue;
		}
		else if (checkArg(argv[i], "top", intValue)) {
			nTop = intValue;
		}
		else if (checkArg(argv[i], "count", intValue)) {
			nCount = intValue;
		}
		else if (checkArg(argv[i], "benchName", stringValue)) {
			benchName = stringValue;
		}
		else if (checkArg(argv[i], "-printHeader", boolValue)) {
			printHeader = true;
		}
		else if (checkArg(argv[i], "-printColumns", boolValue)) {
			printColumns = true;
		}
		else {
			printUsage();
			exit(1);
		}
	}
}


int main (int argc, char const* argv[])
{
	const auto args = Args::parse(argc, argv);

	std::unique_ptr<Executor> exec = createExecutor();
	Timer<int> timer(true);

	// load files
	std::vector<std::string> fileNames = loadFiles(args.fileName);
	if (args.maxFiles >= 0) {
		fileNames.resize(std::min((size_t)args.maxFiles, fileNames.size()));
	}
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
	if (args.printHeader) {
		std::cout <<
			"benchName;time(Loaded Files);count(FilesTotal);count(M LinesTotal);"
			"count(M WordsTotal);avg(M WordsLoad / sec);"
			"time(Top);count(M TopWords);avg(M TopWords / sec);"
			"time(CountWords);count(M CountWords);avg(M CountWords / sec)"
			<< std::endl;
	}
	if (args.printColumns) {
		std::cout << args.benchName << ";"
			<< tLoad << ";" << allFiles.size() << ";" << totalLines / 1000000.f
			<< ";" << totalWords / 1000000.f << ";" << totalWords / (1000.f * tLoad)
			<< ";";
	}
	else {
		printf("%d\ttime(Loaded Files)\n", tLoad);
		printf("%zu\tcount(FilesTotal)\n", allFiles.size());
		printf("%.2f\tcount(M LinesTotal)\n", totalLines / 1000000.f);
		printf("%.2f\tcount(M WordsTotal)\n", totalWords / 1000000.f);
		printf("%.2f\tavg(M WordsLoad / sec)\n", totalWords / (1000.f * tLoad));
	}

	// top
	std::vector<std::string> topFiles;
	size_t topFilesWordCount = 0;
	for (size_t i = 0; i < allFiles.size() && i < args.nTop; i++) {
		TextFile *lf = allFiles[i];
		topFiles.push_back(lf->fileName);
		topFilesWordCount += lf->totalWordCount;
	}
	exec->topWords(topFiles);
	int tTop = timer.stop_get_start();
	if (args.printColumns) {
		std::cout << tTop << ";" << topFilesWordCount / 1000000.f << ";"
			<< topFilesWordCount / (1000.f * tTop) << ";";
	}
	else {
		printf("%d\ttime(Top)\n", tTop);
		printf("%.2f\tcount(M TopWords)\n", topFilesWordCount / 1000000.f);
		printf("%.2f\tavg(M TopWords / sec)\n", topFilesWordCount / (1000.f * tTop));
	}

	// count
	std::vector<std::string> countWords;
	const std::vector<std::string> someWords = {
		"the", "of", "to", "and", "you", "that", "was", "with", "they", "have",
		"for", "from"
	};
	countWords.resize(args.nCount);
	for (size_t i = 0; i < countWords.size(); ++i) {
		countWords[i] = someWords[i%someWords.size()];
	}
	exec->countWords(countWords);
	int tCount = timer.stop_get_start();
	if (args.printColumns) {
		std::cout << tCount << ";" << totalWords * countWords.size() / 1000000.f
			<< ";" << totalWords * countWords.size() / (1000.f * tCount)
			<< std::endl;
	}
	else {
		printf("%d\ttime(CountWords)\n", tCount);
		printf("%.2f\tcount(M CountWords)\n", totalWords * countWords.size() / 1000000.f);
		printf("%.2f\tavg(M CountWords / sec)\n", totalWords * countWords.size() / (1000.f * tCount));
	}

	fflush(stdout);

	return 0;
}
