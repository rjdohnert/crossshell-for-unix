/*
 * Copyright (c) 2025, R. J. Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "traversal_engine.hpp"
#include "path_helper.hpp"
#include <vector>
#include <algorithm>
#include <cctype>

namespace dirtree {

void TreeTraversalEngine::PrintTree(const fs::path& currentPath, const std::string& prefix, int currentLevel,
                                    const TreeOptions& config, TreeStats& stats, std::ostream& out) {
    if (config.maxLevel != -1 && currentLevel >= config.maxLevel) {
        return;
    }

    std::vector<fs::directory_entry> entries;
    try {
        auto opts = fs::directory_options::skip_permission_denied;
        for (const auto& entry : fs::directory_iterator(currentPath, opts)) {
            if (!config.showAll && PathHelper::IsHidden(entry)) {
                continue;
            }

            if (config.dirsOnly && !entry.is_directory()) {
                continue;
            }

            if (!config.extensions.empty() && entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                bool match = false;
                for (const auto& targetExt : config.extensions) {
                    std::string t = targetExt;
                    if (!t.empty() && t[0] == '.') t = t.substr(1);
                    std::transform(t.begin(), t.end(), t.begin(), ::tolower);
                    if (t == ext) { match = true; break; }
                }
                if (!match) continue;
            }

            entries.push_back(entry);
        }
    } catch (const std::exception&) {
        return;
    }

    std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
        if (a.is_directory() != b.is_directory()) {
            return a.is_directory() > b.is_directory();
        }
        std::string nameA = a.path().filename().string();
        std::string nameB = b.path().filename().string();
        std::transform(nameA.begin(), nameA.end(), nameA.begin(), ::tolower);
        std::transform(nameB.begin(), nameB.end(), nameB.begin(), ::tolower);
        return nameA < nameB;
    });

    size_t count = entries.size();
    for (size_t i = 0; i < count; ++i) {
        const auto& entry = entries[i];
        bool isLast = (i == count - 1);

        std::string branch = config.useAscii ? (isLast ? "`-- " : "|-- ") : (isLast ? "└── " : "├── ");
        std::string name = PathHelper::PathToUtf8(entry.path().filename());

        if (entry.is_directory()) {
            stats.dirCount++;
        } else {
            stats.fileCount++;
        }

        if (config.useColor) {
            out << Color::GRAY << prefix << branch << Color::RESET;
        } else {
            out << prefix << branch;
        }

        std::string color = config.useColor ? PathHelper::GetFileColor(entry) : "";
        out << color << name;
        if (config.useColor) {
            out << Color::RESET;
        }

        if (config.showSizes && entry.is_regular_file()) {
            try {
                std::uintmax_t sz = entry.file_size();
                if (config.useColor) {
                    out << Color::GRAY << " (" << PathHelper::FormatSize(sz) << ")" << Color::RESET;
                } else {
                    out << " (" << PathHelper::FormatSize(sz) << ")";
                }
            } catch (...) {}
        }

        out << "\n";

        if (entry.is_directory()) {
            std::string childPrefix = prefix + (config.useAscii ? (isLast ? "    " : "|   ") : (isLast ? "    " : "│   "));
            PrintTree(entry.path(), childPrefix, currentLevel + 1, config, stats, out);
        }
    }
}

} // namespace dirtree
