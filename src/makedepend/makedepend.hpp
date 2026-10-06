#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef MAKEDEPEND_HPP
#define MAKEDEPEND_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

constexpr const char* PROGRAM_NAME = "makedepend";
constexpr const char* PROGRAM_VERSION = "3.1.0";
constexpr const char* DEFAULT_DELIMITER = "# DO NOT DELETE THIS LINE -- make depend depends on it.";

struct PathHelper {
    static std::string to_lower(const std::string& p) {
        std::string s = p;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return std::tolower(c);
        });
        return s;
    }

    static std::string normalize(const std::string& p) {
        fs::path path(p);
        std::string s = path.lexically_normal().string();
        std::replace(s.begin(), s.end(), '\\', '/');
        return s;
    }

    static std::string lower_key(const std::string& p) {
        return to_lower(normalize(p));
    }
};

struct CaseInsensitivePathCompare {
    bool operator()(const std::string& a, const std::string& b) const {
        return PathHelper::lower_key(a) < PathHelper::lower_key(b);
    }
};

struct LineCleanerState {
    bool inBlockComment = false;
    bool inRawString = false;
    std::string rawDelimiter;
};

struct MacroDef {
    std::string name;
    bool isFunctionLike = false;
    std::vector<std::string> params;
    bool isVariadic = false;
    std::string body;
};

struct CondBranch {
    bool active = true;
    bool parentActive = true;
    bool branchTaken = false;
};

#endif // MAKEDEPEND_HPP
