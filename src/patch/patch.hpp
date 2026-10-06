#ifndef PATCH_HPP
#define PATCH_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

struct DiffLine {
    char type = ' ';
    std::string text;
};

struct Hunk {
    int oldStart = 0, oldCount = 0;
    int newStart = 0, newCount = 0;
    std::vector<DiffLine> lines;
};

struct FilePatch {
    std::string oldPath;
    std::string newPath;
    std::vector<Hunk> hunks;
};

class PathUtils {
public:
    static std::string StripPath(const std::string& path, int stripCount);
    static std::string TrimCRLF(const std::string& s);
};

class PatchParser {
public:
    static void ParseHunkHeader(const std::string& header, Hunk& hunk);
    static std::vector<FilePatch> ParseUnifiedDiff(std::istream& in);
};

#endif // PATCH_HPP
