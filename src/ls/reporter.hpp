#ifndef REPORTER_HPP
#define REPORTER_HPP

#include "ls.hpp"
#include "options.hpp"
#include <string>
#include <vector>

class LsReporter {
public:
    static std::string jsonEscape(const std::string& value);
    static std::string csvEscape(const std::string& value);
    static std::string getTypeName(const FileItem& item);
    static std::string getStructuredTimestamp(const FileItem& item);

    static void printStructuredEntries(const std::vector<FileItem>& items, const std::string& format);
    static void printLong(const std::vector<FileItem>& items, const ListingOptions& options);
    static void printColumns(const std::vector<FileItem>& items, const ListingOptions& options, int termWidth);
    static std::string decorateName(const FileItem& item, const ListingOptions& options);
};

#endif // REPORTER_HPP
