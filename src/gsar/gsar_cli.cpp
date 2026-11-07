#include "gsar_cli.hpp"

#include "gsar_pattern.hpp"

#include <filesystem>
#include <iostream>

namespace {
bool IsOption(const std::string& value) {
    return value.size() > 1 && value[0] == '-';
}

bool ReadNextValue(int argc, char* argv[], int& index, std::string& value,
                   const char* option, std::string& error) {
    if (index + 1 >= argc || argv[index + 1] == nullptr ||
        IsOption(argv[index + 1])) {
        error = std::string("missing value for ") + option;
        return false;
    }
    value = argv[++index];
    return true;
}

bool ParsePatternValue(const std::string& value, std::vector<uint8_t>& output,
                       std::string& error) {
    if (!PatternParser::parse(value, output, error)) return false;
    return true;
}

bool ValidateBackupExtension(const std::string& extension, std::string& error) {
    if (extension.empty() || extension == "." || extension == ".." ||
        extension.find_first_of("/\\:") != std::string::npos) {
        error = "backup extension must be a non-empty suffix without path separators";
        return false;
    }
    return true;
}

}

void CommandLineParser::printHelp(const std::string& executableName) {
    std::cout << "Usage: " << executableName
              << " -sPATTERN [-rPATTERN] [options] [input [output]]\n"
                 "       "
              << executableName << " -sPATTERN -F [options] < input > output\n\n"
                 "Searches and replaces byte patterns in binary or text data.\n"
                 "Without -r, matching input is passed through unchanged.\n"
                 "With replacement and input files but no output path, files are\n"
                 "replaced in place using a temporary file.\n\n"
                 "Options:\n"
                 "  -sPATTERN, -s PATTERN  Required search pattern.\n"
                 "  -rPATTERN, -r          Replace matches; bare -r deletes them.\n"
                 "  -i, --ignore-case      ASCII case-insensitive byte matching.\n"
                 "  -c, --count            Count matches without writing transformed data.\n"
                 "  -l, --list             List only files with matches.\n"
                 "  -b, --offsets          Print match offsets to standard error.\n"
                 "  -x, --hex              Print matching bytes in hex to standard error.\n"
                 "  -v, --verbose          Print per-file summaries to standard error.\n"
                 "  -f, --force            Allow replacing an existing output file.\n"
                 "  -F, --filter           Read stdin and write stdout.\n"
                 "  -oFILE, --output FILE  Write one input file to a separate output file.\n"
                 "  -u, --backup           Back up in-place files using the .bak suffix.\n"
                 "  -BEXT, --backup-ext E  Use a custom in-place backup suffix.\n"
                 "  -h, --help             Show this help.\n"
                 "  -V, --version          Show version.\n\n"
                 "Escape sequences (in search and replacement patterns):\n"
                 "  \\a \\b \\e \\f \\n \\r \\t \\v  ASCII control bytes\n"
                 "  \\\\                       Literal backslash\n"
                 "  \\OOO                     Octal byte (0-255)\n"
                 "  \\xHH                     One hexadecimal byte\n"
                 "  \\dDDD                    Decimal byte (0-255)\n"
                 "  :: :n :r :t :b :a :e :0  Colon escape forms; :: is a literal colon\n"
                 "  :xHH...                  One or more complete hexadecimal byte pairs\n\n"
                 "Examples:\n"
                 "  "
              << executableName << " -s\\r\\n -r\\n input.txt output.txt\n"
                 "  "
              << executableName << " -i -sold -rnew -u source.cpp\n"
                 "  "
              << executableName << " -s:xDEADBEEF -b -x binary.exe\n"
                 "  type input.txt | "
              << executableName << " -sfoo -rbar -F > output.txt\n";
}

bool CommandLineParser::parse(int argc, char* argv[], Options& options,
                              std::string& error) {
    options = Options{};
    error.clear();
    std::vector<std::filesystem::path> positional;
    bool positionalOnly = false;
    bool explicitOutput = false;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i] == nullptr ? "" : argv[i];
        if (argument.empty()) {
            error = "empty argument is not a valid input path or option";
            return false;
        }
        if (!positionalOnly && argument == "--") {
            positionalOnly = true;
            continue;
        }
        if (!positionalOnly &&
            (argument == "-h" || argument == "--help" || argument == "/?" ||
             argument == "-?")) {
            options.showHelp = true;
            continue;
        }
        if (!positionalOnly && (argument == "-V" || argument == "--version")) {
            options.showVersion = true;
            continue;
        }
        if (!positionalOnly && (argument == "-i" || argument == "--ignore-case")) {
            options.caseInsensitive = true;
            continue;
        }
        if (!positionalOnly && (argument == "-c" || argument == "--count")) {
            options.countOnly = true;
            continue;
        }
        if (!positionalOnly && (argument == "-l" || argument == "--list")) {
            options.listFilesOnly = true;
            continue;
        }
        if (!positionalOnly && (argument == "-b" || argument == "--offsets")) {
            options.showOffsets = true;
            continue;
        }
        if (!positionalOnly && (argument == "-x" || argument == "--hex")) {
            options.displayHex = true;
            continue;
        }
        if (!positionalOnly && (argument == "-v" || argument == "--verbose")) {
            options.verbose = true;
            continue;
        }
        if (!positionalOnly && (argument == "-f" || argument == "--force")) {
            options.forceOverwrite = true;
            continue;
        }
        if (!positionalOnly &&
            (argument == "-F" || argument == "--filter")) {
            options.filterPipeMode = true;
            continue;
        }
        if (!positionalOnly && (argument == "-u" || argument == "--backup")) {
            options.createBackup = true;
            continue;
        }
        if (!positionalOnly && argument == "-r") {
            options.hasReplace = true;
            continue;
        }
        if (!positionalOnly && argument.rfind("-r", 0) == 0 &&
            argument.size() > 2) {
            options.hasReplace = true;
            if (!ParsePatternValue(argument.substr(2), options.replacePattern, error)) {
                return false;
            }
            continue;
        }
        if (!positionalOnly && argument.rfind("--replace=", 0) == 0) {
            options.hasReplace = true;
            if (!ParsePatternValue(argument.substr(10), options.replacePattern, error)) {
                return false;
            }
            continue;
        }
        if (!positionalOnly && argument == "--replace") {
            std::string value;
            if (!ReadNextValue(argc, argv, i, value, "--replace", error)) return false;
            options.hasReplace = true;
            if (!ParsePatternValue(value, options.replacePattern, error)) return false;
            continue;
        }
        if (!positionalOnly &&
            (argument == "-s" || argument == "--search" ||
             argument.rfind("--search=", 0) == 0 ||
             (argument.rfind("-s", 0) == 0 && argument.size() > 2))) {
            std::string value;
            if (argument == "-s" || argument == "--search") {
                if (!ReadNextValue(argc, argv, i, value, argument.c_str(), error)) return false;
            } else if (argument.rfind("--search=", 0) == 0) {
                value = argument.substr(9);
            } else {
                value = argument.substr(2);
            }
            if (value.empty()) {
                error = "search pattern must not be empty";
                return false;
            }
            if (!ParsePatternValue(value, options.searchPattern, error)) return false;
            options.hasSearch = true;
            continue;
        }
        if (!positionalOnly && argument.rfind("-B", 0) == 0) {
            options.createBackup = true;
            if (argument.size() > 2) options.backupExtension = argument.substr(2);
            if (!ValidateBackupExtension(options.backupExtension, error)) return false;
            continue;
        }
        if (!positionalOnly && argument.rfind("--backup-ext=", 0) == 0) {
            options.createBackup = true;
            options.backupExtension = argument.substr(13);
            if (!ValidateBackupExtension(options.backupExtension, error)) return false;
            continue;
        }
        if (!positionalOnly && argument == "--backup-ext") {
            options.createBackup = true;
            if (!ReadNextValue(argc, argv, i, options.backupExtension,
                               "--backup-ext", error) ||
                !ValidateBackupExtension(options.backupExtension, error)) {
                return false;
            }
            continue;
        }
        if (!positionalOnly &&
            (argument == "-o" || argument == "--output" ||
             argument.rfind("--output=", 0) == 0 ||
             (argument.rfind("-o", 0) == 0 && argument.size() > 2))) {
            std::string value;
            if (argument == "-o" || argument == "--output") {
                if (!ReadNextValue(argc, argv, i, value, argument.c_str(), error)) return false;
            } else if (argument.rfind("--output=", 0) == 0) {
                value = argument.substr(9);
            } else {
                value = argument.substr(2);
            }
            if (value.empty()) {
                error = "output path must not be empty";
                return false;
            }
            options.outputFile = std::filesystem::path(value);
            explicitOutput = true;
            continue;
        }

        if (!positionalOnly && argument[0] == '-') {
            error = "unknown option: " + argument;
            return false;
        }
        positional.emplace_back(argument);
    }

    if (options.showHelp || options.showVersion) {
        if (options.showHelp && options.showVersion) {
            error = "help and version cannot be requested together";
            return false;
        }
        if (argc != 2) {
            error = "help or version cannot be combined with other arguments";
            return false;
        }
        return true;
    }
    if (!options.hasSearch || options.searchPattern.empty()) {
        error = "a non-empty search pattern is required";
        return false;
    }
    if (options.countOnly && options.hasReplace) {
        error = "count-only mode cannot be combined with replacement";
        return false;
    }
    if (options.listFilesOnly &&
        (options.hasReplace || options.countOnly || options.filterPipeMode ||
         explicitOutput)) {
        error = "list mode requires search-only file input";
        return false;
    }
    if (options.createBackup && (!options.hasReplace || options.filterPipeMode)) {
        error = "backups require in-place file replacement";
        return false;
    }
    if (options.countOnly && explicitOutput) {
        error = "count-only mode cannot write an output file";
        return false;
    }
    if (options.filterPipeMode && (!positional.empty() || explicitOutput)) {
        error = "filter mode reads standard input and cannot use file paths";
        return false;
    }

    if (explicitOutput) {
        if (positional.size() != 1) {
            error = "an explicit output path requires exactly one input file";
            return false;
        }
        options.inputFiles = std::move(positional);
    } else if (options.filterPipeMode) {
        if (!positional.empty()) {
            error = "filter mode reads standard input and cannot take input files";
            return false;
        }
    } else if (positional.size() == 2) {
        options.inputFiles.push_back(positional[0]);
        options.outputFile = positional[1];
    } else {
        options.inputFiles = std::move(positional);
    }

    if (options.outputFile.empty() && options.inputFiles.empty()) {
        options.filterPipeMode = true;
    }
    if (!options.outputFile.empty() && options.inputFiles.empty()) {
        error = "an output file requires an input file";
        return false;
    }
    if (options.outputFile.empty() && options.forceOverwrite) {
        error = "force mode requires an output file";
        return false;
    }
    if (options.countOnly && !options.outputFile.empty()) {
        error = "count-only mode cannot write an output file";
        return false;
    }
    if (options.listFilesOnly && !options.outputFile.empty()) {
        error = "list mode cannot write an output file";
        return false;
    }
    if (options.createBackup && options.inputFiles.empty()) {
        error = "backups require one or more input files";
        return false;
    }

    return true;
}
