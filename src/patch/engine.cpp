#include "engine.hpp"

// ============================================================================
// PathUtils Implementation
// ============================================================================

std::string PathUtils::StripPath(const std::string& path, int stripCount) {
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

std::string PathUtils::TrimCRLF(const std::string& s) {
    std::string r = s;
    while (!r.empty() && (r.back() == '\r' || r.back() == '\n')) {
        r.pop_back();
    }
    return r;
}

// ============================================================================
// PatchParser Implementation
// ============================================================================

void PatchParser::ParseHunkHeader(const std::string& header, Hunk& hunk) {
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

std::vector<FilePatch> PatchParser::ParseUnifiedDiff(std::istream& in) {
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

// ============================================================================
// PatchEngine Implementation
// ============================================================================

void PatchEngine::ReverseFilePatch(FilePatch& patch) {
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

bool PatchEngine::ApplyPatchToFile(const FilePatch& patch, const PatchOptions& opts) {
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
