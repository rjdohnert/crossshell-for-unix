#include "env_arguments.hpp"

#include <cstddef>
#include <cwctype>

bool ArgumentTokenizer::SplitString(const std::wstring& value,
                                    std::vector<std::wstring>& tokens,
                                    std::wstring& error) {
    std::wstring current;
    bool tokenStarted = false;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escaping = false;

    for (wchar_t character : value) {
        if (escaping) {
            switch (character) {
                case L'n': current.push_back(L'\n'); break;
                case L'r': current.push_back(L'\r'); break;
                case L't': current.push_back(L'\t'); break;
                case L'_': current.push_back(L' '); break;
                default: current.push_back(character); break;
            }
            tokenStarted = true;
            escaping = false;
            continue;
        }

        if (character == L'\\' && !inSingleQuote) {
            escaping = true;
            tokenStarted = true;
            continue;
        }
        if (character == L'\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            tokenStarted = true;
            continue;
        }
        if (character == L'"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            tokenStarted = true;
            continue;
        }
        if (iswspace(character) && !inSingleQuote && !inDoubleQuote) {
            if (tokenStarted) {
                tokens.push_back(std::move(current));
                current.clear();
                tokenStarted = false;
            }
            continue;
        }
        current.push_back(character);
        tokenStarted = true;
    }

    if (escaping) {
        error = L"split string ends with an incomplete escape.";
        return false;
    }
    if (inSingleQuote || inDoubleQuote) {
        error = L"split string contains an unterminated quote.";
        return false;
    }
    if (tokenStarted) tokens.push_back(std::move(current));
    return true;
}

ParseResult CommandLineParser::Parse(int argc, wchar_t* argv[]) {
    ParseResult result;
    std::vector<std::wstring> args;
    args.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index) {
        if (!argv[index]) {
            result.error = L"received a null command-line argument.";
            return result;
        }
        args.emplace_back(argv[index]);
    }

    bool formatWasSet = false;
    if (!ParseTokens(args, result.options, result.error, formatWasSet, 0)) {
        return result;
    }
    if (result.options.showHelp || result.options.showVersion) {
        result.success = true;
        return result;
    }
    if (result.options.nullDelimiter &&
        result.options.outputFormat != OutputFormat::Plain) {
        result.error = L"--null cannot be combined with structured output.";
        return result;
    }
    if (!result.options.commandAndArgs.empty() &&
        result.options.outputFormat != OutputFormat::Plain) {
        result.error = L"structured output formats can only be used when "
                       L"printing the environment.";
        return result;
    }
    if (!result.options.commandAndArgs.empty() &&
        !result.options.pipeCommand.empty()) {
        result.error = L"--pipe can only be used when printing the environment.";
        return result;
    }
    result.success = true;
    return result;
}

bool CommandLineParser::ParseTokens(const std::vector<std::wstring>& args,
                                    ProgramOptions& options,
                                    std::wstring& error,
                                    bool& formatWasSet,
                                    unsigned int splitDepth) {
    std::size_t index = 0;
    while (index < args.size()) {
        const std::wstring& argument = args[index];
        if (argument == L"--") {
            ++index;
            break;
        }

        if (argument == L"-" || argument == L"-i" ||
            argument == L"--ignore-environment") {
            options.ignoreEnvironment = true;
            ++index;
            continue;
        }
        if (argument == L"-0" || argument == L"--null") {
            options.nullDelimiter = true;
            ++index;
            continue;
        }
        if (argument == L"-v" || argument == L"--verbose") {
            options.verbose = true;
            ++index;
            continue;
        }
        if (argument == L"-?" || argument == L"--help") {
            options.showHelp = true;
            return true;
        }
        if (argument == L"--version") {
            options.showVersion = true;
            return true;
        }

        OutputFormat requestedFormat = OutputFormat::Plain;
        bool isFormat = true;
        if (argument == L"--json") requestedFormat = OutputFormat::Json;
        else if (argument == L"--csv") requestedFormat = OutputFormat::Csv;
        else if (argument == L"--table") requestedFormat = OutputFormat::Table;
        else isFormat = false;
        if (isFormat) {
            if (formatWasSet && options.outputFormat != requestedFormat) {
                error = L"choose only one of --json, --csv, or --table.";
                return false;
            }
            options.outputFormat = requestedFormat;
            formatWasSet = true;
            ++index;
            continue;
        }

        if (argument == L"--pipe") {
            if (!options.pipeCommand.empty()) {
                error = L"--pipe may be specified only once.";
                return false;
            }
            if (index + 1 >= args.size() || args[index + 1].empty()) {
                error = L"--pipe requires a command.";
                return false;
            }
            options.pipeCommand = args[index + 1];
            index += 2;
            continue;
        }

        if (argument == L"-u" || argument == L"--unset") {
            if (index + 1 >= args.size() || args[index + 1].empty()) {
                error = argument + L" requires a variable name.";
                return false;
            }
            options.unsetVariables.push_back(args[index + 1]);
            index += 2;
            continue;
        }
        if (argument.rfind(L"--unset=", 0) == 0) {
            const std::wstring name = argument.substr(8);
            if (name.empty()) {
                error = L"--unset requires a variable name.";
                return false;
            }
            options.unsetVariables.push_back(name);
            ++index;
            continue;
        }
        if (argument.rfind(L"-u", 0) == 0 && argument.size() > 2) {
            options.unsetVariables.push_back(argument.substr(2));
            ++index;
            continue;
        }

        if (argument == L"-C" || argument == L"--chdir") {
            if (index + 1 >= args.size() || args[index + 1].empty()) {
                error = argument + L" requires a directory.";
                return false;
            }
            options.changeDir = args[index + 1];
            index += 2;
            continue;
        }
        if (argument.rfind(L"--chdir=", 0) == 0) {
            options.changeDir = argument.substr(8);
            if (options.changeDir.empty()) {
                error = L"--chdir requires a directory.";
                return false;
            }
            ++index;
            continue;
        }
        if (argument.rfind(L"-C", 0) == 0 && argument.size() > 2) {
            options.changeDir = argument.substr(2);
            ++index;
            continue;
        }

        std::wstring splitString;
        std::size_t nextIndex = index + 1;
        bool isSplitString = false;
        if (argument == L"-S" || argument == L"--split-string") {
            if (nextIndex >= args.size()) {
                error = argument + L" requires a split string.";
                return false;
            }
            splitString = args[nextIndex++];
            isSplitString = true;
        } else if (argument.rfind(L"--split-string=", 0) == 0) {
            splitString = argument.substr(15);
            isSplitString = true;
        } else if (argument.rfind(L"-S", 0) == 0 && argument.size() > 2) {
            splitString = argument.substr(2);
            isSplitString = true;
        }
        if (isSplitString) {
            if (splitDepth >= 8) {
                error = L"split-string expansion exceeds the nesting limit.";
                return false;
            }
            std::vector<std::wstring> expanded;
            if (!ArgumentTokenizer::SplitString(splitString, expanded, error)) {
                return false;
            }
            expanded.insert(expanded.end(), args.begin() +
                static_cast<std::ptrdiff_t>(nextIndex), args.end());
            return ParseTokens(expanded, options, error, formatWasSet,
                               splitDepth + 1);
        }

        if (!argument.empty() && argument.front() == L'-') {
            error = L"invalid option: " + argument;
            return false;
        }

        const std::size_t equals = argument.find(L'=');
        if (equals != std::wstring::npos && equals > 0) {
            options.setVariables.emplace_back(
                argument.substr(0, equals), argument.substr(equals + 1));
            ++index;
            continue;
        }
        break;
    }

    while (index < args.size()) {
        options.commandAndArgs.push_back(args[index++]);
    }
    return true;
}
