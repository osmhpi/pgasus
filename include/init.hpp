#pragma once

#include <string>
#include <vector>
#include <utility>

namespace numa {

typedef std::pair<std::string,std::string> Option;
typedef std::vector<Option> OptionsMap;

bool init(int& argc, const char** &argv);
OptionsMap getInitOptions();

}
