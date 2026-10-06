#ifndef SEARCH_REPORTER_HPP
#define SEARCH_REPORTER_HPP

#include "search.hpp"
#include "terminal_format.hpp"

class SearchReporter {
public:
    static void PrintBanner();
    static void PrintHeader(const SearchOptions& opt);
    static void PrintResult(const fs::directory_entry& entry, bool isDir, bool isReg, bool isSym,
                            uintmax_t fsize, const std::string& attrStr, fs::file_time_type ftime,
                            const SearchOptions& opt);
    static void PrintSummary(size_t totalScanned, size_t matchedFiles, size_t matchedDirs,
                             uintmax_t matchedBytes, double durationMs, const SearchOptions& opt);
};

#endif // SEARCH_REPORTER_HPP
