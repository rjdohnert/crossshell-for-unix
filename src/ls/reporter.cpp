#include "reporter.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

std::string LsReporter::jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (unsigned char ch : value) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (ch < 0x20) {
                    out << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<int>(ch) << std::dec << std::setfill(' ');
                } else {
                    out << static_cast<char>(ch);
                }
                break;
        }
    }
    return out.str();
}

std::string LsReporter::csvEscape(const std::string& value) {
    if (value.find_first_of(",\"\n\r") == std::string::npos) {
        return value;
    }
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped += '"';
    for (char ch : value) {
        if (ch == '"') escaped += "\"\"";
        else escaped += ch;
    }
    escaped += '"';
    return escaped;
}

std::string LsReporter::getTypeName(const FileItem& item) {
    if (item.isDirectory) return "directory";
    if (item.isSymlink) return "symlink";
    if (item.attributes & FILE_ATTRIBUTE_REPARSE_POINT) return "reparse";
    return "file";
}

std::string LsReporter::getStructuredTimestamp(const FileItem& item) {
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(
        item.lastWriteTime - fs::file_time_type::clock::now() + system_clock::now()
    );
    std::time_t tt = system_clock::to_time_t(sctp);
    std::tm tmVal;
    localtime_s(&tmVal, &tt);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmVal);
    return std::string(buf);
}

void LsReporter::printStructuredEntries(const std::vector<FileItem>& items, const std::string& format) {
    if (format == "json") {
        std::cout << "[\n";
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& item = items[i];
            std::cout << "  {\n"
                      << "    \"name\": \"" << jsonEscape(item.name) << "\",\n"
                      << "    \"path\": \"" << jsonEscape(pathToUtf8(item.path)) << "\",\n"
                      << "    \"type\": \"" << jsonEscape(getTypeName(item)) << "\",\n"
                      << "    \"size\": " << item.size << ",\n"
                      << "    \"modified\": \"" << jsonEscape(getStructuredTimestamp(item)) << "\",\n"
                      << "    \"attributes\": \"" << jsonEscape(item.getWindowsModeString()) << "\"\n"
                      << "  }" << ((i + 1 < items.size()) ? "," : "") << "\n";
        }
        std::cout << "]\n";
        return;
    }

    if (format == "csv") {
        std::cout << "name,path,type,size,modified,attributes\n";
        for (const auto& item : items) {
            std::cout << csvEscape(item.name) << ','
                      << csvEscape(pathToUtf8(item.path)) << ','
                      << csvEscape(getTypeName(item)) << ','
                      << csvEscape(std::to_string(item.size)) << ','
                      << csvEscape(getStructuredTimestamp(item)) << ','
                      << csvEscape(item.getWindowsModeString()) << "\n";
        }
        return;
    }

    if (format == "table") {
        std::vector<std::pair<std::string, size_t>> widths = {
            {"name", 4}, {"path", 4}, {"type", 4}, {"size", 4}, {"modified", 8}, {"attributes", 10}
        };
        std::vector<std::vector<std::string>> rows;
        rows.reserve(items.size());
        for (const auto& item : items) {
            rows.push_back({
                item.name,
                pathToUtf8(item.path),
                getTypeName(item),
                std::to_string(item.size),
                getStructuredTimestamp(item),
                item.getWindowsModeString()
            });
        }
        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                widths[i].second = (std::max)(widths[i].second, row[i].length());
            }
        }
        std::cout << std::left;
        for (size_t i = 0; i < widths.size(); ++i) {
            std::cout << std::setw(static_cast<int>(widths[i].second)) << widths[i].first << " | ";
        }
        std::cout << "\n";
        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                std::cout << std::setw(static_cast<int>(widths[i].second)) << row[i] << " | ";
            }
            std::cout << "\n";
        }
    }
}

std::string LsReporter::decorateName(const FileItem& item, const ListingOptions& options) {
    std::string formattedName = item.name;
    if (options.typeIndicator) {
        if (item.isDirectory) formattedName += "/";
        else if (item.isSymlink) formattedName += "@";
        else if (item.attributes & FILE_ATTRIBUTE_DIRECTORY) formattedName += "/";
        else {
            std::string ext = extensionToUtf8(item.path);
            if (ext == ".exe" || ext == ".bat" || ext == ".cmd") formattedName += "*";
        }
    }

    if (!options.color) {
        return formattedName;
    }

    std::string colorCode = ColorTheme::classify(item.path, item.attributes, item.isSymlink);
    return colorCode + formattedName + ColorTheme::RESET;
}

void LsReporter::printLong(const std::vector<FileItem>& items, const ListingOptions& options) {
    uint64_t totalBlocks512 = 0;
    size_t maxOwner = 4, maxDomain = 4, maxSize = 1;

    for (const auto& it : items) {
        totalBlocks512 += (it.size + 511) / 512;
        maxOwner = (std::max)(maxOwner, it.secInfo.owner.length());
        maxDomain = (std::max)(maxDomain, it.secInfo.domain.length());
        maxSize = (std::max)(maxSize, std::to_string(it.size).length());
    }

    std::cout << "total " << totalBlocks512 << "\n";

    for (const auto& it : items) {
        std::cout << ColorTheme::ATTR_COLOR << it.getWindowsModeString() << ColorTheme::RESET << "  "
                  << std::left << std::setw(static_cast<int>(maxDomain)) << it.secInfo.domain << "\\"
                  << std::left << std::setw(static_cast<int>(maxOwner))  << it.secInfo.owner  << "  "
                  << std::right << std::setw(static_cast<int>(maxSize))  << it.size << " "
                  << it.getFormattedTimestamp() << " "
                  << decorateName(it, options) << "\n";
    }
}

void LsReporter::printColumns(const std::vector<FileItem>& items, const ListingOptions& options, int termWidth) {
    if (items.empty()) return;

    std::vector<std::string> rendered;
    size_t maxLen = 0;

    for (const auto& it : items) {
        std::string disp = it.name;
        if (options.typeIndicator) {
            if (it.isDirectory) disp += "/";
            else if (it.isSymlink) disp += "@";
            else if (it.path.extension() == ".exe") disp += "*";
        }
        maxLen = (std::max)(maxLen, disp.length());
        rendered.push_back(decorateName(it, options));
    }

    int colWidth = static_cast<int>(maxLen) + 3;
    int numCols = (std::max)(1, termWidth / colWidth);
    int numRows = static_cast<int>((items.size() + numCols - 1) / numCols);

    for (int r = 0; r < numRows; ++r) {
        for (int c = 0; c < numCols; ++c) {
            size_t idx = c * numRows + r;
            if (idx < items.size()) {
                std::string plain = items[idx].name;
                int pad = colWidth - static_cast<int>(plain.length());
                std::cout << rendered[idx] << std::string((std::max)(1, pad), ' ');
            }
        }
        std::cout << "\n";
    }
}
