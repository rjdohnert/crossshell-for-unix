/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "du_arg_parser.hpp"

#include <charconv>
#include <limits>
#include <string_view>
#include <system_error>

namespace du {
namespace {

bool ParseDepth(std::string_view value, std::size_t& depth) {
    if (value.empty()) return false;
    std::uint64_t parsed = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end ||
        parsed > (std::numeric_limits<std::size_t>::max)()) {
        return false;
    }
    depth = static_cast<std::size_t>(parsed);
    return true;
}

bool SetDepth(std::string_view value, Options& options, std::string& error) {
    std::size_t depth = 0;
    if (!ParseDepth(value, depth)) {
        error = "maximum depth must be a non-negative integer.";
        return false;
    }
    options.maxDepth = depth;
    return true;
}

bool SetFormat(OutputFormat format, Options& options, bool& formatSet,
               std::string& error) {
    if (formatSet && options.outputFormat != format) {
        error = "choose only one of --json, --csv, or --table.";
        return false;
    }
    options.outputFormat = format;
    formatSet = true;
    return true;
}

bool ParseShortCluster(std::string_view argument, Options& options,
                       std::string& error) {
    for (std::size_t index = 1; index < argument.size(); ++index) {
        switch (argument[index]) {
            case 'H':
                options.sizeFormat = SizeFormatMode::HumanReadable;
                break;
            case 'a':
                options.allFiles = true;
                break;
            case 's':
                options.summarizeOnly = true;
                break;
            case 'c':
                options.total = true;
                break;
            case 'k':
                options.sizeFormat = SizeFormatMode::Kilobytes;
                options.blockSize = 1024;
                break;
            case 'm':
                options.sizeFormat = SizeFormatMode::Megabytes;
                options.blockSize = 1024 * 1024;
                break;
            case 'b':
                options.sizeFormat = SizeFormatMode::Bytes;
                options.apparentSize = true;
                break;
            case '0':
                options.nullTerminated = true;
                break;
            case 'L':
                options.dereference = true;
                break;
            default:
                error = "invalid option -- '" +
                    std::string(1, argument[index]) + "'.";
                return false;
        }
    }
    return true;
}

} // namespace

ParseResult ParseArguments(int argc, char* argv[]) {
    ParseResult result;
    bool formatSet = false;
    bool positionalOnly = false;
    bool sawPositionalPath = false;

    for (int index = 1; index < argc; ++index) {
        if (!argv[index]) {
            result.error = "received a null command-line argument.";
            return result;
        }
        const std::string_view argument(argv[index]);
        if (positionalOnly || argument == "-" ||
            argument.empty() || argument.front() != '-') {
            result.options.paths.emplace_back(argument);
            sawPositionalPath = true;
            continue;
        }
        if (argument == "--") {
            positionalOnly = true;
        } else if (argument == "--help" || argument == "-h" ||
                   argument == "-?") {
            result.options.showHelp = true;
            result.success = true;
            return result;
        } else if (argument == "--version" || argument == "-v") {
            result.options.showVersion = true;
            result.success = true;
            return result;
        } else if (argument == "-H" || argument == "--human-readable") {
            result.options.sizeFormat = SizeFormatMode::HumanReadable;
        } else if (argument == "-a" || argument == "--all") {
            result.options.allFiles = true;
        } else if (argument == "-s" || argument == "--summarize") {
            result.options.summarizeOnly = true;
        } else if (argument == "-c" || argument == "--total") {
            result.options.total = true;
        } else if (argument == "-k") {
            result.options.sizeFormat = SizeFormatMode::Kilobytes;
            result.options.blockSize = 1024;
        } else if (argument == "-m") {
            result.options.sizeFormat = SizeFormatMode::Megabytes;
            result.options.blockSize = 1024 * 1024;
        } else if (argument == "-b" || argument == "--bytes") {
            result.options.sizeFormat = SizeFormatMode::Bytes;
            result.options.apparentSize = true;
        } else if (argument == "--apparent-size") {
            result.options.apparentSize = true;
        } else if (argument == "-0" || argument == "--null") {
            result.options.nullTerminated = true;
        } else if (argument == "-L" || argument == "--dereference") {
            result.options.dereference = true;
        } else if (argument == "-d" || argument == "--max-depth") {
            if (index + 1 >= argc) {
                result.error = "option requires a non-negative depth argument.";
                return result;
            }
            if (!SetDepth(argv[++index], result.options, result.error)) {
                return result;
            }
        } else if (argument.rfind("-d=", 0) == 0 ||
                   argument.rfind("--max-depth=", 0) == 0) {
            const std::size_t equals = argument.find('=');
            if (!SetDepth(argument.substr(equals + 1), result.options,
                          result.error)) {
                return result;
            }
        } else if (argument.rfind("--exclude=", 0) == 0) {
            const std::string pattern(argument.substr(10));
            if (pattern.empty()) {
                result.error = "--exclude requires a non-empty pattern.";
                return result;
            }
            result.options.excludePatterns.push_back(pattern);
        } else if (argument == "--exclude") {
            if (index + 1 >= argc || std::string_view(argv[index + 1]).empty()) {
                result.error = "option --exclude requires a non-empty pattern.";
                return result;
            }
            result.options.excludePatterns.emplace_back(argv[++index]);
        } else if (argument.rfind("--files0-from=", 0) == 0) {
            if (!result.options.files0From.empty()) {
                result.error = "--files0-from may be specified only once.";
                return result;
            }
            result.options.files0From = std::string(argument.substr(14));
            if (result.options.files0From.empty()) {
                result.error = "--files0-from requires a file name.";
                return result;
            }
        } else if (argument == "--files0-from") {
            if (!result.options.files0From.empty() || index + 1 >= argc ||
                std::string_view(argv[index + 1]).empty()) {
                result.error = "--files0-from requires one file name.";
                return result;
            }
            result.options.files0From = argv[++index];
        } else if (argument == "--json") {
            if (!SetFormat(OutputFormat::Json, result.options, formatSet,
                           result.error)) return result;
        } else if (argument == "--csv") {
            if (!SetFormat(OutputFormat::Csv, result.options, formatSet,
                           result.error)) return result;
        } else if (argument == "--table") {
            if (!SetFormat(OutputFormat::Table, result.options, formatSet,
                           result.error)) return result;
        } else if (argument == "--pipe") {
            if (!result.options.pipeCommand.empty()) {
                result.error = "--pipe may be specified only once.";
                return result;
            }
            if (index + 1 >= argc || std::string_view(argv[index + 1]).empty()) {
                result.error = "option --pipe requires a command.";
                return result;
            }
            result.options.pipeCommand = argv[++index];
        } else if (argument.size() > 1 && argument.front() == '-') {
            if (!ParseShortCluster(argument, result.options, result.error)) {
                return result;
            }
        } else {
            result.options.paths.emplace_back(argument);
            sawPositionalPath = true;
        }
    }

    if (!result.options.files0From.empty() && sawPositionalPath) {
        result.error = "--files0-from cannot be combined with file operands.";
        return result;
    }
    if (result.options.outputFormat != OutputFormat::Human &&
        result.options.nullTerminated) {
        result.error = "--null is available only with human-readable output.";
        return result;
    }

    result.success = true;
    return result;
}

} // namespace du
