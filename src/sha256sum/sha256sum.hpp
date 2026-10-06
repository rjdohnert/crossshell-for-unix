#ifndef SHA256SUM_HPP
#define SHA256SUM_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <memory>

struct Sha256Result {
    std::string filename;
    std::string hash;
    bool isBinary{true};
    bool error{false};
};

#endif // SHA256SUM_HPP
