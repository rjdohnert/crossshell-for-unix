#ifndef SEARCH_HPP
#define SEARCH_HPP

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <regex>
#include <chrono>
#include <ctime>
#include <sstream>
#include <filesystem>
#include <optional>
#include <cctype>
#include <algorithm>
#include <set>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")

namespace fs = std::filesystem;

struct SearchOptions {
    std::vector<fs::path> targetPaths;
    std::string namePattern = "";
    std::vector<std::string> extensions;
    char typeFilter = 'a'; // 'a' = all, 'f' = file, 'd' = directory
    bool isRegex = false;
    bool caseInsensitive = true;
    bool exactMatch = false;
    int maxDepth = -1; // -1 = infinite

    // Size filters (in bytes)
    std::optional<uintmax_t> minSize;
    std::optional<uintmax_t> maxSize;

    // Time filters
    std::optional<std::chrono::system_clock::time_point> modifiedAfter;
    std::optional<std::chrono::system_clock::time_point> modifiedBefore;

    // Windows attributes
    bool includeHidden = true;
    bool hiddenOnly = false;
    bool readonlyOnly = false;
    bool systemOnly = false;

    // Display options
    bool summaryOnly = false;
    bool bareOutput = false;
    bool showAttributes = true;
    bool searchUserDirs = false;
};

#endif // SEARCH_HPP
