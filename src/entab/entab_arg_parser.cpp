/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "entab_arg_parser.hpp"

#include <charconv>
#include <limits>
#include <string_view>
#include <system_error>

namespace {

bool ParsePositiveSize(std::string_view text, std::size_t& value) {
    if (text.empty()) return false;
    unsigned long long parsed = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed == 0 ||
        parsed > (std::numeric_limits<std::size_t>::max)()) {
        return false;
    }
    value = static_cast<std::size_t>(parsed);
    return true;
}

bool ParseTabSpecification(std::string_view specification,
                           EntabOptions& options, std::string& error) {
    std::vector<std::size_t> values;
    std::string token;
    bool commaPending = false;
    auto addToken = [&]() {
        if (token.empty()) return true;
        std::size_t value = 0;
        if (!ParsePositiveSize(token, value) ||
            (!values.empty() && value <= values.back())) {
            return false;
        }
        values.push_back(value);
        token.clear();
        commaPending = false;
        return true;
    };

    for (char character : specification) {
        if (character >= '0' && character <= '9') {
            token.push_back(character);
        } else if (character == ',') {
            if (!addToken() || values.empty() || commaPending) {
                error = "tab stops must be positive, representable, and "
                        "strictly increasing.";
                return false;
            }
            commaPending = true;
        } else if (character == ' ' || character == '\t') {
            if (!addToken()) {
                error = "tab stops must be positive, representable, and "
                        "strictly increasing.";
                return false;
            }
        } else {
            error = "tab specification contains an invalid character.";
            return false;
        }
    }
    if (!addToken() || commaPending || values.empty()) {
        error = "tab specification must contain positive tab stops in "
                "strictly increasing order.";
        return false;
    }

    if (values.size() == 1) {
        options.tabWidth = values.front();
        options.tabStops.clear();
    } else {
        options.tabStops = std::move(values);
    }
    return true;
}

bool SetOutputFormat(EntabOutputFormat requested, EntabOptions& options,
                     bool& formatWasSet, std::string& error) {
    if (formatWasSet && options.outputFormat != requested) {
        error = "choose only one of --json, --csv, or --table.";
        return false;
    }
    options.outputFormat = requested;
    formatWasSet = true;
    return true;
}

bool ParseTabArgument(std::string_view text, EntabOptions& options,
                      bool& tabStopsEnableAll, std::string& error) {
    if (!ParseTabSpecification(text, options, error)) return false;
    tabStopsEnableAll = true;
    return true;
}

} // namespace

EntabParseResult ParseEntabArguments(int argc, char* argv[]) {
    EntabParseResult result;
    bool formatWasSet = false;
    bool rawFilesOnly = false;
    bool allWasRequested = false;
    bool tabStopsEnableAll = false;

    for (int index = 1; index < argc; ++index) {
        if (!argv[index]) {
            result.error = "received a null command-line argument.";
            return result;
        }
        const std::string_view argument(argv[index]);
        if (rawFilesOnly) {
            result.options.files.push_back({std::string(argument), false});
            continue;
        }
        if (argument == "-") {
            result.options.files.push_back({"-", true});
            continue;
        }
        if (argument.empty() || argument.front() != '-') {
            result.options.files.push_back({std::string(argument), false});
            continue;
        }
        if (argument == "--") {
            rawFilesOnly = true;
        } else if (argument == "-h" || argument == "--help" ||
                   argument == "/?") {
            result.options.showHelp = true;
            result.success = true;
            return result;
        } else if (argument == "-v" || argument == "--version") {
            result.options.showVersion = true;
            result.success = true;
            return result;
        } else if (argument == "--json") {
            if (!SetOutputFormat(EntabOutputFormat::Json, result.options,
                                 formatWasSet, result.error)) return result;
        } else if (argument == "--csv") {
            if (!SetOutputFormat(EntabOutputFormat::Csv, result.options,
                                 formatWasSet, result.error)) return result;
        } else if (argument == "--table") {
            if (!SetOutputFormat(EntabOutputFormat::Table, result.options,
                                 formatWasSet, result.error)) return result;
        } else if (argument == "-a" || argument == "--all") {
            allWasRequested = true;
        } else if (argument == "--first-only") {
            result.options.firstOnly = true;
        } else if (argument.rfind("--tabs=", 0) == 0) {
            if (!ParseTabArgument(argument.substr(7), result.options,
                                  tabStopsEnableAll, result.error)) {
                return result;
            }
        } else if (argument == "-t" || argument == "--tabs") {
            if (index + 1 >= argc ||
                !ParseTabArgument(argv[++index], result.options,
                                  tabStopsEnableAll, result.error)) {
                if (result.error.empty()) {
                    result.error = std::string(argument) +
                                   " requires a tab specification.";
                }
                return result;
            }
        } else if (argument.rfind("-t", 0) == 0 && argument.size() > 2) {
            if (!ParseTabArgument(argument.substr(2), result.options,
                                  tabStopsEnableAll, result.error)) {
                return result;
            }
        } else if (argument.size() > 1 && argument.front() == '-' &&
                   argument[1] >= '0' && argument[1] <= '9') {
            if (!ParseTabArgument(argument.substr(1), result.options,
                                  tabStopsEnableAll, result.error)) {
                return result;
            }
        } else if (argument == "--pipe") {
            if (!result.options.pipeCommand.empty()) {
                result.error = "--pipe may be specified only once.";
                return result;
            }
            if (index + 1 >= argc ||
                std::string_view(argv[index + 1]).empty()) {
                result.error = "--pipe requires a command.";
                return result;
            }
            result.options.pipeCommand = argv[++index];
        } else {
            result.error = "invalid option: " + std::string(argument);
            return result;
        }
    }

    result.options.convertAll = !result.options.firstOnly &&
        (allWasRequested || tabStopsEnableAll);
    result.success = true;
    return result;
}
