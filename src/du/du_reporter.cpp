/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "du_reporter.hpp"

#include "du_size_formatter.hpp"

#include <iomanip>
#include <ostream>
#include <string>
#include <string_view>

namespace du {
namespace {

std::string PathToUtf8(const std::filesystem::path& path) {
    return path.u8string();
}

std::size_t Utf8SequenceLength(std::string_view value, std::size_t index) {
    const auto byte = [&value](std::size_t offset) {
        return static_cast<unsigned char>(value[offset]);
    };
    const unsigned char first = byte(index);
    const std::size_t remaining = value.size() - index;
    if (first >= 0xc2 && first <= 0xdf && remaining >= 2 &&
        (byte(index + 1) & 0xc0) == 0x80) {
        return 2;
    }
    if (first >= 0xe0 && first <= 0xef && remaining >= 3) {
        const unsigned char second = byte(index + 1);
        const unsigned char third = byte(index + 2);
        const bool validSecond =
            (second & 0xc0) == 0x80 &&
            (first != 0xe0 || second >= 0xa0) &&
            (first != 0xed || second <= 0x9f);
        return validSecond && (third & 0xc0) == 0x80 ? 3 : 0;
    }
    if (first >= 0xf0 && first <= 0xf4 && remaining >= 4) {
        const unsigned char second = byte(index + 1);
        const bool validSecond =
            (second & 0xc0) == 0x80 &&
            (first != 0xf0 || second >= 0x90) &&
            (first != 0xf4 || second <= 0x8f);
        return validSecond && (byte(index + 2) & 0xc0) == 0x80 &&
               (byte(index + 3) & 0xc0) == 0x80 ? 4 : 0;
    }
    return 0;
}

std::string EscapeJson(std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string output;
    output.reserve(value.size());
    for (std::size_t index = 0; index < value.size();) {
        const unsigned char character =
            static_cast<unsigned char>(value[index]);
        if (character >= 0x80) {
            const std::size_t length = Utf8SequenceLength(value, index);
            if (length != 0) {
                output.append(value.substr(index, length));
                index += length;
            } else {
                output += "\\u00";
                output += hex[character >> 4];
                output += hex[character & 0x0f];
                ++index;
            }
            continue;
        }
        switch (character) {
            case '\\': output += "\\\\"; break;
            case '"': output += "\\\""; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (character < 0x20) {
                    output += "\\u00";
                    output += hex[character >> 4];
                    output += hex[character & 0x0f];
                } else {
                    output += static_cast<char>(character);
                }
        }
        ++index;
    }
    return output;
}

std::string EscapeDisplayPath(std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string output;
    for (std::size_t index = 0; index < value.size();) {
        const unsigned char character =
            static_cast<unsigned char>(value[index]);
        if (character == '\t') {
            output += "\\t";
        } else if (character == '\r') {
            output += "\\r";
        } else if (character == '\n') {
            output += "\\n";
        } else if (character < 0x20 || character == 0x7f) {
            output += "\\x";
            output += hex[character >> 4];
            output += hex[character & 0x0f];
        } else if (character >= 0x80) {
            const std::size_t length = Utf8SequenceLength(value, index);
            if (length == 0) {
                output += "\\x";
                output += hex[character >> 4];
                output += hex[character & 0x0f];
                ++index;
            } else {
                output.append(value.substr(index, length));
                index += length;
            }
        } else {
            output += static_cast<char>(character);
            ++index;
        }
    }
    return output;
}

std::string EscapeCsv(std::string_view value) {
    std::string output;
    output.reserve(value.size() + 3);
    const std::size_t first = value.find_first_not_of(" \t\r\n\v\f");
    const bool formula = first != std::string_view::npos &&
        (value[first] == '=' || value[first] == '+' ||
         value[first] == '-' || value[first] == '@');
    output += '"';
    if (formula) output += '\'';
    for (std::size_t index = 0; index < value.size();) {
        const unsigned char character =
            static_cast<unsigned char>(value[index]);
        if (character == '"') {
            output += "\"\"";
            ++index;
        } else if (character < 0x20 || character == 0x7f) {
            output += "\\x";
            output += "0123456789abcdef"[character >> 4];
            output += "0123456789abcdef"[character & 0x0f];
            ++index;
        } else if (character >= 0x80) {
            const std::size_t length = Utf8SequenceLength(value, index);
            if (length == 0) {
                output += "\\x";
                output += "0123456789abcdef"[character >> 4];
                output += "0123456789abcdef"[character & 0x0f];
                ++index;
            } else {
                output.append(value.substr(index, length));
                index += length;
            }
        } else {
            output += static_cast<char>(character);
            ++index;
        }
    }
    output += '"';
    return output;
}

} // namespace

DuReporter::DuReporter(const Options& options, std::ostream& output)
    : options_(options), output_(output) {}

void DuReporter::Begin() {
    switch (options_.outputFormat) {
        case OutputFormat::Human:
            break;
        case OutputFormat::Json:
            output_ << "{\"entries\":[";
            break;
        case OutputFormat::Csv:
            output_ << "Path,Bytes,Size\n";
            break;
        case OutputFormat::Table:
            output_ << "Bytes\tSize\tPath\n";
            break;
    }
}

void DuReporter::WriteEntry(const UsageEntry& entry) {
    const std::string path = PathToUtf8(entry.path);
    const std::string formattedSize = FormatSize(entry.bytes, options_);
    switch (options_.outputFormat) {
        case OutputFormat::Human:
            output_ << formattedSize << '\t' << EscapeDisplayPath(path)
                    << (options_.nullTerminated ? '\0' : '\n');
            break;
        case OutputFormat::Json:
            if (entryCount_ != 0) output_ << ',';
            output_ << "{\"path\":\"" << EscapeJson(path)
                    << "\",\"bytes\":" << entry.bytes
                    << ",\"size\":\"" << EscapeJson(formattedSize) << "\"}";
            break;
        case OutputFormat::Csv:
            output_ << EscapeCsv(path) << ',' << entry.bytes << ','
                    << EscapeCsv(formattedSize) << '\n';
            break;
        case OutputFormat::Table:
            output_ << entry.bytes << '\t' << formattedSize << '\t'
                    << EscapeDisplayPath(path) << '\n';
            break;
    }
    ++entryCount_;
}

void DuReporter::Finish(const ScanSummary& summary) {
    if (options_.outputFormat == OutputFormat::Json) {
        output_ << "],\"totalBytes\":" << summary.totalBytes
                << ",\"files\":" << summary.fileCount
                << ",\"skipped\":" << summary.skippedCount
                << ",\"errors\":" << summary.errorCount << "}\n";
    }
}

} // namespace du
