/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <regex>
#include <memory>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// Command Line Configuration
// ============================================================================
struct RenameOptions {
    bool verbose       = false; // -v, --verbose
    bool dryRun        = false; // -n, --no-act
    bool interactive   = false; // -i, --interactive
    bool noOverwrite   = false; // -o, --no-overwrite
    bool replaceAll    = false; // -a, --all
    bool replaceLast   = false; // -l, --last
    bool useRegex      = false; // -e, --regex
    bool ignoreCase    = false; // -c, --ignore-case
    bool showHelp      = false; // -h, --help
    bool showVersion   = false; // -V, --version

    std::string searchPattern;
    std::string replacement;
    std::vector<fs::path> targetFiles;
};

// ============================================================================
// Strategy Pattern for String Replacement
// ============================================================================
class IRenameStrategy {
public:
    virtual ~IRenameStrategy() = default;
    [[nodiscard]] virtual std::string transform(const std::string& input) const = 0;
};

class SubstringStrategy : public IRenameStrategy {
private:
    std::string from_;
    std::string to_;
    bool replaceAll_;
    bool replaceLast_;
    bool ignoreCase_;

    [[nodiscard]] static bool caseInsensitiveCompare(char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    }

    [[nodiscard]] size_t findMatch(const std::string& str, size_t pos) const {
        if (from_.empty()) return std::string::npos;
        if (!ignoreCase_) {
            return str.find(from_, pos);
        }
        auto it = std::search(str.begin() + pos, str.end(),
                              from_.begin(), from_.end(),
                              caseInsensitiveCompare);
        if (it != str.end()) {
            return std::distance(str.begin(), it);
        }
        return std::string::npos;
    }

    [[nodiscard]] size_t rfindMatch(const std::string& str) const {
        if (from_.empty()) return std::string::npos;
        if (!ignoreCase_) {
            return str.rfind(from_);
        }
        auto it = std::find_end(str.begin(), str.end(),
                                from_.begin(), from_.end(),
                                caseInsensitiveCompare);
        if (it != str.end()) {
            return std::distance(str.begin(), it);
        }
        return std::string::npos;
    }

public:
    SubstringStrategy(std::string from, std::string to, bool all, bool last, bool ignoreCase)
        : from_(std::move(from)), to_(std::move(to)),
          replaceAll_(all), replaceLast_(last), ignoreCase_(ignoreCase) {}

    std::string transform(const std::string& input) const override {
        if (from_.empty()) return input;

        std::string result = input;
        if (replaceLast_) {
            size_t pos = rfindMatch(result);
            if (pos != std::string::npos) {
                result.replace(pos, from_.length(), to_);
            }
        } else if (replaceAll_) {
            size_t pos = 0;
            while ((pos = findMatch(result, pos)) != std::string::npos) {
                result.replace(pos, from_.length(), to_);
                pos += to_.length();
            }
        } else {
            size_t pos = findMatch(result, 0);
            if (pos != std::string::npos) {
                result.replace(pos, from_.length(), to_);
            }
        }
        return result;
    }
};

class RegexStrategy : public IRenameStrategy {
private:
    std::regex regexEngine_;
    std::string replacement_;
    bool replaceAll_;

public:
    RegexStrategy(const std::string& pattern, std::string replacement, bool all, bool ignoreCase)
        : replacement_(std::move(replacement)), replaceAll_(all) {
        auto flags = std::regex_constants::ECMAScript;
        if (ignoreCase) {
            flags |= std::regex_constants::icase;
        }
        regexEngine_ = std::regex(pattern, flags);
    }

    std::string transform(const std::string& input) const override {
        if (replaceAll_) {
            return std::regex_replace(input, regexEngine_, replacement_);
        } else {
            return std::regex_replace(input, regexEngine_, replacement_,
                                      std::regex_constants::format_first_only);
        }
    }
};

// ============================================================================
// Windows Wildcard / Glob Expansion Engine
// ============================================================================
class WildcardExpander {
private:
    static bool matchesWildcard(std::string_view text, std::string_view pattern) {
        size_t t = 0, p = 0, starIdx = std::string_view::npos, match = 0;
        while (t < text.size()) {
            if (p < pattern.size() && (pattern[p] == '?' ||
                std::tolower(static_cast<unsigned char>(text[t])) ==
                std::tolower(static_cast<unsigned char>(pattern[p])))) {
                ++t;
                ++p;
            } else if (p < pattern.size() && pattern[p] == '*') {
                starIdx = p;
                match = t;
                ++p;
            } else if (starIdx != std::string_view::npos) {
                p = starIdx + 1;
                ++match;
                t = match;
            } else {
                return false;
            }
        }
        while (p < pattern.size() && pattern[p] == '*') {
            ++p;
        }
        return p == pattern.size();
    }

public:
    static std::vector<fs::path> expand(const std::vector<std::string>& inputs) {
        std::vector<fs::path> resolved;

        for (const auto& raw : inputs) {
            if (raw.find_first_of("*?") == std::string::npos) {
                resolved.emplace_back(raw);
                continue;
            }

            fs::path p(raw);
            fs::path dir = p.has_parent_path() ? p.parent_path() : fs::current_path();
            std::string pattern = p.filename().string();

            std::error_code ec;
            if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
                continue;
            }

            bool matchedAny = false;
            for (const auto& entry : fs::directory_iterator(dir, ec)) {
                std::string entryName = entry.path().filename().string();
                if (matchesWildcard(entryName, pattern)) {
                    resolved.push_back(entry.path());
                    matchedAny = true;
                }
            }

            if (!matchedAny) {
                // If nothing matched, retain literal argument to let caller handle standard file error
                resolved.emplace_back(raw);
            }
        }
        return resolved;
    }
};

// ============================================================================
// Comprehensive Help Formatter
// ============================================================================
class HelpFormatter {
public:
    static void printUsage(std::ostream& os) {
        os << "Usage: rename [options] <expression> <replacement> <file...>\n"
           << "Try 'rename --help' for complete documentation and examples.\n";
    }

    static void printHelp(std::ostream& os) {
        os << R"HELP(
NAME
    rename - rename multiple files using pattern replacement

SYNOPSIS
    rename [OPTIONS] <expression> <replacement> <file...>
    rename -h | --help
    rename -V | --version

DESCRIPTION
    The rename utility renames the specified files by replacing matches of
    <expression> with <replacement> in each file's base name. The directory
    structure leading up to the file remains untouched.

    This program is a native Windows implementation compatible with the
    traditional Unix/HP-UX rename utility semantics, augmented with Windows
    command-line wildcard expansion and modern regular expression support.

OPTIONS
    -v, --verbose
        Display diagnostic output detailing every file renamed.

    -n, --no-act, --dry-run
        Simulate the renaming operations without committing any modifications
        to the filesystem. Use alongside -v to inspect pending operations.

    -i, --interactive
        Prompt for confirmation before overwriting an existing destination file.

    -o, --no-overwrite
        Do not overwrite existing destination files; collisions will be skipped.

    -a, --all
        Replace all occurrences of <expression> in the file name, rather than
        just the first occurrence.

    -l, --last
        Replace the last occurrence of <expression> rather than the first.
        (Ignored if combined with -a or -e).

    -c, --ignore-case
        Perform case-insensitive matching for <expression>.

    -e, --regex
        Treat <expression> as an ECMAScript regular expression. Capture groups
        such as $1, $2, etc., can be referenced in <replacement>.

    -h, --help
        Display this comprehensive manual page and exit.

    -V, --version
        Display utility version, build information, and exit.

WINDOWS SPECIFIC BEHAVIOR
    - Wildcard Expansion:
        Because Windows cmd.exe does not expand wildcards like Unix shells do,
        this utility automatically expands '*' and '?' wildcards across file
        arguments.

    - Case Renaming:
        Renaming a file to change its casing (e.g., lowercase to uppercase) is
        safely handled using native Win32 MoveFileEx replacement semantics.

    - Path Separators:
        Both forward ('/') and backward ('\\') slashes are supported as path
        separators.

EXIT STATUS
    0   Successful completion.
    1   One or more file renaming operations failed.
    2   Invalid command-line options or insufficient arguments.

EXAMPLES
    1. Replace the first occurrence of '.jpeg' with '.jpg' across files:
       > rename .jpeg .jpg *.jpeg

    2. Dry run: preview changing occurrences of 'draft' to 'final':
       > rename -v -n draft final draft_*.txt

    3. Replace ALL underscores with hyphens in file names:
       > rename -a "_" "-" *.*

    4. Replace the LAST occurrence of 'old' with 'new':
       > rename -l old new my_old_doc_old.txt

    5. Case-insensitive renaming with confirmation before overwriting:
       > rename -i -c data DATA data*.csv

    6. Regular expression capture and reordering:
       > rename -e "([a-z]+)_([0-9]+)" "$2_$1" *.log
)HELP";
    }

    static void printVersion(std::ostream& os) {
        os << "rename 1.2.0\n"
           << "Copyright (C) 2026, Roberto J. Dohnert.\n";
    }
};

// ============================================================================
// Command Line Parser
// ============================================================================
class CommandLineParser {
public:
    static bool parse(int argc, char* argv[], RenameOptions& options) {
        std::vector<std::string> positionalArgs;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                options.showHelp = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                options.showVersion = true;
                return true;
            } else if (arg == "-v" || arg == "--verbose") {
                options.verbose = true;
            } else if (arg == "-n" || arg == "--no-act" || arg == "--dry-run") {
                options.dryRun = true;
            } else if (arg == "-i" || arg == "--interactive") {
                options.interactive = true;
            } else if (arg == "-o" || arg == "--no-overwrite") {
                options.noOverwrite = true;
            } else if (arg == "-a" || arg == "--all") {
                options.replaceAll = true;
            } else if (arg == "-l" || arg == "--last") {
                options.replaceLast = true;
            } else if (arg == "-c" || arg == "--ignore-case") {
                options.ignoreCase = true;
            } else if (arg == "-e" || arg == "--regex") {
                options.useRegex = true;
            } else if (arg.rfind("--", 0) == 0) {
                std::cerr << "rename: unrecognized option '" << arg << "'\n";
                return false;
            } else if (arg.rfind("-", 0) == 0 && arg.length() > 1) {
                // Handle bundled short options (e.g. -va)
                for (size_t c = 1; c < arg.length(); ++c) {
                    switch (arg[c]) {
                        case 'v': options.verbose = true; break;
                        case 'n': options.dryRun = true; break;
                        case 'i': options.interactive = true; break;
                        case 'o': options.noOverwrite = true; break;
                        case 'a': options.replaceAll = true; break;
                        case 'l': options.replaceLast = true; break;
                        case 'c': options.ignoreCase = true; break;
                        case 'e': options.useRegex = true; break;
                        default:
                            std::cerr << "rename: invalid option -- '" << arg[c] << "'\n";
                            return false;
                    }
                }
            } else {
                positionalArgs.push_back(arg);
            }
        }

        if (positionalArgs.size() < 3) {
            std::cerr << "rename: missing operand\n";
            return false;
        }

        options.searchPattern = positionalArgs[0];
        options.replacement   = positionalArgs[1];

        std::vector<std::string> rawFiles(positionalArgs.begin() + 2, positionalArgs.end());
        options.targetFiles = WildcardExpander::expand(rawFiles);

        return true;
    }
};

// ============================================================================
// File Renaming Engine
// ============================================================================
class FileRenamer {
private:
    RenameOptions options_;
    std::unique_ptr<IRenameStrategy> strategy_;
    size_t processedCount_ = 0;
    size_t renamedCount_   = 0;
    size_t errorCount_     = 0;
    size_t skippedCount_   = 0;

    [[nodiscard]] bool askUserConfirmation(const fs::path& target) const {
        std::cout << "rename: overwrite '" << target.string() << "'? (y/n): ";
        std::string response;
        if (std::getline(std::cin, response)) {
            return !response.empty() && (response[0] == 'y' || response[0] == 'Y');
        }
        return false;
    }

    bool executeMove(const fs::path& source, const fs::path& dest) {
#ifdef _WIN32
        // Win32 MoveFileExW provides atomic replacement and handles casing changes
        DWORD flags = MOVEFILE_COPY_ALLOWED;
        if (!options_.noOverwrite) {
            flags |= MOVEFILE_REPLACE_EXISTING;
        }

        if (!MoveFileExW(source.c_str(), dest.c_str(), flags)) {
            DWORD error = GetLastError();
            std::cerr << "rename: cannot rename '" << source.string()
                      << "' to '" << dest.string() << "': Error " << error << "\n";
            return false;
        }
        return true;
#else
        std::error_code ec;
        fs::rename(source, dest, ec);
        if (ec) {
            std::cerr << "rename: cannot rename '" << source.string()
                      << "' to '" << dest.string() << "': " << ec.message() << "\n";
            return false;
        }
        return true;
#endif
    }

public:
    explicit FileRenamer(RenameOptions options) : options_(std::move(options)) {
        if (options_.useRegex) {
            strategy_ = std::make_unique<RegexStrategy>(
                options_.searchPattern,
                options_.replacement,
                options_.replaceAll,
                options_.ignoreCase
            );
        } else {
            strategy_ = std::make_unique<SubstringStrategy>(
                options_.searchPattern,
                options_.replacement,
                options_.replaceAll,
                options_.replaceLast,
                options_.ignoreCase
            );
        }
    }

    int execute() {
        for (const auto& filePath : options_.targetFiles) {
            ++processedCount_;

            std::error_code ec;
            if (!fs::exists(filePath, ec)) {
                std::cerr << "rename: cannot access '" << filePath.string()
                          << "': No such file or directory\n";
                ++errorCount_;
                continue;
            }

            std::string oldFilename = filePath.filename().string();
            std::string newFilename;

            try {
                newFilename = strategy_->transform(oldFilename);
            } catch (const std::regex_error& e) {
                std::cerr << "rename: regular expression error: " << e.what() << "\n";
                return 2;
            }

            // If replacement produced no change, skip
            if (oldFilename == newFilename) {
                continue;
            }

            fs::path destination = filePath.parent_path() / newFilename;

            // Handle destination collisions (excluding same-file case changes on Windows)
            bool sameFile = false;
            if (fs::exists(destination, ec)) {
                std::error_code eqEc;
                sameFile = fs::equivalent(filePath, destination, eqEc);

                if (!sameFile) {
                    if (options_.noOverwrite) {
                        if (options_.verbose) {
                            std::cout << "skipping '" << filePath.string()
                                      << "' (target exists)\n";
                        }
                        ++skippedCount_;
                        continue;
                    }

                    if (options_.interactive && !askUserConfirmation(destination)) {
                        if (options_.verbose) {
                            std::cout << "skipping '" << filePath.string() << "'\n";
                        }
                        ++skippedCount_;
                        continue;
                    }
                }
            }

            if (options_.verbose || options_.dryRun) {
                std::cout << "'" << filePath.string() << "' -> '"
                          << destination.string() << "'\n";
            }

            if (options_.dryRun) {
                ++renamedCount_;
                continue;
            }

            if (executeMove(filePath, destination)) {
                ++renamedCount_;
            } else {
                ++errorCount_;
            }
        }

        return (errorCount_ > 0) ? 1 : 0;
    }
};

// ============================================================================
// Application Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);

    RenameOptions options;
    if (!CommandLineParser::parse(argc, argv, options)) {
        HelpFormatter::printUsage(std::cerr);
        return 2;
    }

    if (options.showHelp) {
        HelpFormatter::printHelp(std::cout);
        return 0;
    }

    if (options.showVersion) {
        HelpFormatter::printVersion(std::cout);
        return 0;
    }

    FileRenamer engine(std::move(options));
    return engine.execute();
}