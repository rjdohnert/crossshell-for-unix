#pragma once

#include "which.hpp"

class PathCandidateResolver {
public:
    static std::string toLower(std::string str);

    static std::vector<std::string> split(const std::string& str, char delim);

    static std::string trimQuotes(std::string str);

    static bool isExecutable(const fs::path& p);

    static std::vector<fs::path> getCandidates(const fs::path& basePath, const std::vector<std::string>& pathexts);
};
