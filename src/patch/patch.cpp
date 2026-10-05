/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

// ============================================================================
// 1. DIFF DATA MODEL
// ============================================================================

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

// ============================================================================
// 2. PATH UTILITIES
// ============================================================================

class PathUtils {
public:
    static std::string StripPath(const std::string& path, int stripCount) {
        if (stripCount <= 0 || path.empty()) return path;
        size_t pos = 0;
        int count = 0;
        while (count < stripCount && pos < path.size()) {
            size_t next = path.find_first_of("/\\", pos);
            if (next == std::string::npos) {
                return path.substr(pos);
            }
            pos = next + 1;
            count++;
        }
        return (pos < path.size()) ? path.substr(pos) : path;
    }

    static std::string TrimCRLF(const std::string& s) {
        std::string r = s;
        while (!r.empty() && (r.back() == '\r' || r.back() == '\n')) {
            r.pop_back();
        }
        return r;
    }
};

// ============================================================================
// 3. UNIFIED DIFF PARSER
// ============================================================================

class PatchParser {
public:
    static void ParseHunkHeader(const std::string& header, Hunk& hunk) {
        size_t oldPos = header.find('-');
        size_t newPos = header.find('+');
        if (oldPos == std::string::npos || newPos == std::string::npos) return;

        auto parseRange = [](const std::string& str, int& start, int& count) {
            size_t comma = str.find(',');
            if (comma != std::string::npos) {
                start = std::stoi(str.substr(0, comma));
                count = std::stoi(str.substr(comma + 1));
            } else {
                start = std::stoi(str);
                count = 1;
            }
        };

        size_t oldEnd = header.find(' ', oldPos);
        size_t newEnd = header.find(' ', newPos);

        parseRange(header.substr(oldPos + 1, oldEnd - oldPos - 1), hunk.oldStart, hunk.oldCount);
        parseRange(header.substr(newPos + 1, newEnd - newPos - 1), hunk.newStart, hunk.newCount);
    }

    static std::vector<FilePatch> ParseUnifiedDiff(std::istream& in) {
        std::vector<FilePatch> patches;
        std::string line;
        FilePatch currentPatch;
        Hunk currentHunk;
        bool inHunk = false;

        auto finalizeHunk = [&]() {
            if (inHunk && !currentHunk.lines.empty()) {
                currentPatch.hunks.push_back(currentHunk);
                currentHunk = Hunk();
                inHunk = false;
            }
        };

        auto finalizePatch = [&]() {
            finalizeHunk();
            if (!currentPatch.hunks.empty()) {
                patches.push_back(currentPatch);
                currentPatch = FilePatch();
            }
        };

        while (std::getline(in, line)) {
            std::string trimmed = PathUtils::TrimCRLF(line);

            if (trimmed.rfind("--- ", 0) == 0) {
                finalizePatch();
                std::string path = trimmed.substr(4);
                size_t tabPos = path.find_first_of("\t\r\n");
                if (tabPos != std::string::npos) path = path.substr(0, tabPos);
                currentPatch.oldPath = path;
            } else if (trimmed.rfind("+++ ", 0) == 0) {
                std::string path = trimmed.substr(4);
                size_t tabPos = path.find_first_of("\t\r\n");
                if (tabPos != std::string::npos) path = path.substr(0, tabPos);
                currentPatch.newPath = path;
            } else if (trimmed.rfind("@@ ", 0) == 0) {
                finalizeHunk();
                inHunk = true;
                ParseHunkHeader(trimmed, currentHunk);
            } else if (inHunk) {
                if (trimmed.empty()) {
                    currentHunk.lines.push_back({' ', ""});
                } else if (trimmed[0] == '-' || trimmed[0] == '+' || trimmed[0] == ' ') {
                    currentHunk.lines.push_back({trimmed[0], trimmed.substr(1)});
                } else if (trimmed.rfind("\\ No newline at end of file", 0) == 0) {
                    // Skip
                } else {
                    finalizeHunk();
                }
            }
        }
        finalizePatch();
        return patches;
    }
};

// ============================================================================
// 4. PATCH APPLICATION ENGINE
// ============================================================================

struct PatchOptions {
    int stripCount = 0;
    std::string patchFile = "";
    std::string outputFile = "";
    std::string backupSuffix = ".orig";
    std::string changeDir = "";
    bool reverse = false;
    bool makeBackup = false;
    bool dryRun = false;
    bool removeEmpty = false;
    std::vector<std::string> positionalArgs;
};

class PatchEngine {
public:
    static void ReverseFilePatch(FilePatch& patch) {
        std::swap(patch.oldPath, patch.newPath);
        for (auto& hunk : patch.hunks) {
            std::swap(hunk.oldStart, hunk.newStart);
            std::swap(hunk.oldCount, hunk.newCount);
            for (auto& line : hunk.lines) {
                if (line.type == '-') line.type = '+';
                else if (line.type == '+') line.type = '-';
            }
        }
    }

    static bool ApplyPatchToFile(const FilePatch& patch, const PatchOptions& opts) {
        std::string targetPath = opts.outputFile.empty() ? 
            PathUtils::StripPath(patch.newPath != "/dev/null" ? patch.newPath : patch.oldPath, opts.stripCount) : 
            opts.outputFile;

        std::replace(targetPath.begin(), targetPath.end(), '/', '\\');

        std::cout << (opts.dryRun ? "checking file " : "patching file ") << targetPath;
        if (opts.reverse) std::cout << " (reversed)";
        std::cout << "\n";

        std::vector<std::string> fileLines;
        if (fs::exists(targetPath)) {
            std::ifstream file(targetPath);
            std::string line;
            while (std::getline(file, line)) {
                fileLines.push_back(PathUtils::TrimCRLF(line));
            }
        }

        int lineOffset = 0;
        bool allHunksApplied = true;
        std::vector<std::string> rejLines;

        for (size_t hIdx = 0; hIdx < patch.hunks.size(); ++hIdx) {
            const auto& hunk = patch.hunks[hIdx];

            std::vector<std::string> expectedBefore;
            std::vector<std::string> replacement;

            for (const auto& line : hunk.lines) {
                if (line.type == ' ' || line.type == '-') expectedBefore.push_back(line.text);
                if (line.type == ' ' || line.type == '+') replacement.push_back(line.text);
            }

            int targetLine = (hunk.oldStart > 0 ? hunk.oldStart - 1 : 0) + lineOffset;
            int bestMatch = -1;

            for (int searchOffset = 0; searchOffset <= static_cast<int>(fileLines.size()) + 100; ++searchOffset) {
                int candidates[] = { targetLine + searchOffset, targetLine - searchOffset };
                for (int candidate : candidates) {
                    if (candidate < 0 || candidate + static_cast<int>(expectedBefore.size()) > static_cast<int>(fileLines.size())) continue;

                    bool match = true;
                    for (size_t i = 0; i < expectedBefore.size(); ++i) {
                        if (fileLines[candidate + i] != expectedBefore[i]) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        bestMatch = candidate;
                        break;
                    }
                }
                if (bestMatch != -1) break;
            }

            if (bestMatch != -1) {
                lineOffset += (bestMatch - targetLine) + (static_cast<int>(replacement.size()) - static_cast<int>(expectedBefore.size()));
                fileLines.erase(fileLines.begin() + bestMatch, fileLines.begin() + bestMatch + expectedBefore.size());
                fileLines.insert(fileLines.begin() + bestMatch, replacement.begin(), replacement.end());
                if (opts.dryRun) {
                    std::cout << "Hunk #" << (hIdx + 1) << " succeeded at line " << (bestMatch + 1) << ".\n";
                }
            } else {
                std::cerr << "Hunk #" << (hIdx + 1) << " FAILED at line " << hunk.oldStart << ".\n";
                allHunksApplied = false;

                rejLines.push_back("@@ -" + std::to_string(hunk.oldStart) + "," + std::to_string(hunk.oldCount) +
                                   " +" + std::to_string(hunk.newStart) + "," + std::to_string(hunk.newCount) + " @@");
                for (const auto& l : hunk.lines) {
                    rejLines.push_back(std::string(1, l.type) + l.text);
                }
            }
        }

        if (opts.dryRun) {
            std::cout << "[dry-run] no files modified.\n";
            return allHunksApplied;
        }

        if (opts.makeBackup && fs::exists(targetPath)) {
            std::string backupPath = targetPath + opts.backupSuffix;
            fs::copy_file(targetPath, backupPath, fs::copy_options::overwrite_existing);
            std::cout << "created backup file " << backupPath << "\n";
        }

        if (opts.removeEmpty && fileLines.empty()) {
            if (fs::exists(targetPath)) fs::remove(targetPath);
            return allHunksApplied;
        }

        std::ofstream outFile(targetPath);
        for (size_t i = 0; i < fileLines.size(); ++i) {
            outFile << fileLines[i] << (i + 1 < fileLines.size() ? "\n" : "");
        }
        outFile.close();

        if (!rejLines.empty()) {
            std::string rejPath = targetPath + ".rej";
            std::ofstream rejFile(rejPath);
            for (const auto& l : rejLines) rejFile << l << "\n";
            std::cerr << "1 out of " << patch.hunks.size() << " hunks FAILED -- saving rejects to file " << rejPath << "\n";
        }

        return allHunksApplied;
    }
};

// ============================================================================
// 5. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

class OptionParser {
public:
    static void PrintHelp() {
        std::cout << R"(patch(1)                CrossShell for UNIX Reference Manual                  patch(1)

    NAME
        patch - apply a diff file to an original

    SYNOPSIS
        patch [OPTIONS] [ORIGFILE [PATCHFILE]]

    DESCRIPTION
        patch takes a patch file containing a difference listing produced by
        diff and applies those differences to one or more original files,
        generating patched versions.

    OPTIONS
        -p NUM, --strip NUM
            Strip NUM leading path components from file names found in diff.

        -i FILE, --input FILE
            Read patch from FILE instead of standard input.

        -o FILE, --output FILE
            Write output to FILE instead of modifying files in place.

        -R, --reverse
            Assume that the patch was created with the old and new files
            reversed; swap old and new changes.

        -b, --backup
            Make backup copies of files before modifying them.

        -z SUFFIX, --suffix SUFFIX
            Use SUFFIX instead of .orig as the backup file extension.

        -E, --remove-empty-files
            Remove output files that become empty after patching.

        -d DIR, --directory DIR
            Change working directory to DIR before applying patches.

        --dry-run
            Test applying patch without modifying files on disk.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version information.

    EXAMPLES
        patch -p1 -i fix.patch
            Apply unified diff stripping 1 leading path segment.

        patch -p0 -R -b -i fix.patch
            Revert patch and create backup files with .orig suffix.

        patch -p1 --dry-run -i fix.patch
            Check patch application without writing changes.

    CrossShell for UNIX                                                    patch(1)
)";
    }

    static void PrintVersion() {
        std::cout << "patch v1.0.0\n";
    }

    bool Parse(int argc, char* argv[], PatchOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--") {
                for (int j = i + 1; j < argc; ++j) {
                    opts.positionalArgs.push_back(argv[j]);
                }
                break;
            } else if (arg == "-p" || arg == "--strip") {
                if (++i < argc) {
                    try {
                        opts.stripCount = std::stoi(argv[i]);
                    } catch (...) {
                        std::cerr << "patch: invalid strip count '" << argv[i] << "'\n";
                        return false;
                    }
                }
            } else if (arg.rfind("-p", 0) == 0 && arg.size() > 2) {
                try {
                    opts.stripCount = std::stoi(arg.substr(2));
                } catch (...) {
                    std::cerr << "patch: invalid strip count '" << arg.substr(2) << "'\n";
                    return false;
                }
            } else if (arg == "-i" || arg == "--input") {
                if (++i < argc) opts.patchFile = argv[i];
            } else if (arg == "-o" || arg == "--output") {
                if (++i < argc) opts.outputFile = argv[i];
            } else if (arg == "-b" || arg == "--backup") {
                opts.makeBackup = true;
            } else if (arg == "-z" || arg == "--suffix") {
                if (++i < argc) { opts.backupSuffix = argv[i]; opts.makeBackup = true; }
            } else if (arg == "-R" || arg == "--reverse") {
                opts.reverse = true;
            } else if (arg == "-E" || arg == "--remove-empty-files") {
                opts.removeEmpty = true;
            } else if (arg == "--dry-run") {
                opts.dryRun = true;
            } else if (arg == "-d" || arg == "--directory") {
                if (++i < argc) opts.changeDir = argv[i];
            } else if (arg == "-h" || arg == "--help" || arg == "/?") {
                PrintHelp();
                exitEarly = true;
                return true;
            } else if (arg == "--version" || arg == "-V") {
                PrintVersion();
                exitEarly = true;
                return true;
            } else if (arg[0] != '-') {
                opts.positionalArgs.push_back(arg);
            } else {
                std::cerr << "patch: unknown option '" << arg << "'\n";
                std::cerr << "Try 'patch --help' for options.\n";
                return false;
            }
        }
        return true;
    }
};

class PatchApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]) {
        PatchOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        if (!opts.changeDir.empty()) {
            fs::current_path(opts.changeDir);
        }

        if (opts.positionalArgs.size() >= 1 && opts.outputFile.empty()) {
            opts.outputFile = opts.positionalArgs[0];
        }
        if (opts.positionalArgs.size() >= 2 && opts.patchFile.empty()) {
            opts.patchFile = opts.positionalArgs[1];
        }

        std::istream* patchStream = &std::cin;
        std::ifstream fileIn;

        if (!opts.patchFile.empty()) {
            fileIn.open(opts.patchFile);
            if (!fileIn.is_open()) {
                std::cerr << "patch: cannot open patch file '" << opts.patchFile << "'\n";
                return 1;
            }
            patchStream = &fileIn;
        }

        std::vector<FilePatch> patches = PatchParser::ParseUnifiedDiff(*patchStream);
        if (patches.empty()) {
            std::cerr << "patch: no valid unified diff hunks found.\n";
            return 1;
        }

        bool success = true;
        for (auto& patch : patches) {
            if (opts.reverse) {
                PatchEngine::ReverseFilePatch(patch);
            }
            if (!PatchEngine::ApplyPatchToFile(patch, opts)) {
                success = false;
            }
        }

        return success ? 0 : 1;
    }
};

int main(int argc, char* argv[]) {
    PatchApplication app;
    return app.Run(argc, argv);
}
