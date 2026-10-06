#include "search_engine.hpp"

int SearchEngine::Execute(const SearchOptions& opt) {
    if (!m_matcher.Initialize(opt.namePattern, opt.isRegex, opt.exactMatch, opt.caseInsensitive)) {
        return 1;
    }

    SearchReporter::PrintHeader(opt);

    size_t totalScanned = 0;
    size_t matchedFiles = 0;
    size_t matchedDirs  = 0;
    uintmax_t matchedBytes = 0;
    auto startTime = std::chrono::high_resolution_clock::now();

    for (const auto& root : opt.targetPaths) {
        std::error_code ec;
        if (!fs::exists(root, ec)) {
            std::cerr << ConsoleTerminal::Red << "Warning: " << ConsoleTerminal::Reset << "Path not found: " << root.string() << "\n";
            continue;
        }

        auto it_opts = fs::directory_options::skip_permission_denied;
        auto iter = fs::recursive_directory_iterator(root, it_opts, ec);
        auto end = fs::recursive_directory_iterator();

        for (; iter != end; iter.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }

            totalScanned++;

            if (opt.maxDepth >= 0 && iter.depth() > opt.maxDepth) {
                iter.pop();
                continue;
            }

            const auto& entry = *iter;
            bool isDir = entry.is_directory(ec);
            bool isReg = entry.is_regular_file(ec);
            bool isSym = entry.is_symlink(ec);

            // 1. Filter: Type
            if (opt.typeFilter == 'f' && !isReg) continue;
            if (opt.typeFilter == 'd' && !isDir) continue;

            // 2. Filter: Windows Attributes
            DWORD rawAttr = 0;
            std::string attrStr = AttributeInspector::GetAttributesString(entry.path(), &rawAttr);
            bool isHidden = (rawAttr != INVALID_FILE_ATTRIBUTES) && (rawAttr & FILE_ATTRIBUTE_HIDDEN);
            bool isReadonly = (rawAttr != INVALID_FILE_ATTRIBUTES) && (rawAttr & FILE_ATTRIBUTE_READONLY);
            bool isSys = (rawAttr != INVALID_FILE_ATTRIBUTES) && (rawAttr & FILE_ATTRIBUTE_SYSTEM);

            if (!opt.includeHidden && isHidden) continue;
            if (opt.hiddenOnly && !isHidden) continue;
            if (opt.readonlyOnly && !isReadonly) continue;
            if (opt.systemOnly && !isSys) continue;

            // 3. Filter: Extensions
            if (!opt.extensions.empty()) {
                std::string fileExt = entry.path().extension().string();
                std::transform(fileExt.begin(), fileExt.end(), fileExt.begin(), ::tolower);
                bool extMatch = false;
                for (const auto& ext : opt.extensions) {
                    if (fileExt == ext) {
                        extMatch = true;
                        break;
                    }
                }
                if (!extMatch) continue;
            }

            // 4. Filter: Name / Regex / Wildcard
            if (m_matcher.HasPattern()) {
                std::string filename = entry.path().filename().string();
                if (!m_matcher.Matches(filename)) continue;
            }

            // 5. Filter: Size
            uintmax_t fsize = 0;
            if (isReg) {
                fsize = entry.file_size(ec);
                if (ec) { fsize = 0; ec.clear(); }
                if (opt.minSize && fsize < *opt.minSize) continue;
                if (opt.maxSize && fsize > *opt.maxSize) continue;
            } else if (isDir && opt.minSize && *opt.minSize == 0 && opt.maxSize && *opt.maxSize == 0) {
                auto subIt = fs::directory_iterator(entry.path(), it_opts, ec);
                if (subIt != fs::directory_iterator()) continue;
            }

            // 6. Filter: Timestamps
            auto ftime = entry.last_write_time(ec);
            if (!ec) {
                auto sysTime = DateTimeFormatter::ToSystemTime(ftime);
                if (opt.modifiedAfter && sysTime < *opt.modifiedAfter) continue;
                if (opt.modifiedBefore && sysTime > *opt.modifiedBefore) continue;
            }

            // Metric tallies
            if (isDir) matchedDirs++;
            if (isReg) {
                matchedFiles++;
                matchedBytes += fsize;
            }

            // Output Result
            SearchReporter::PrintResult(entry, isDir, isReg, isSym, fsize, attrStr, ftime, opt);
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    SearchReporter::PrintSummary(totalScanned, matchedFiles, matchedDirs, matchedBytes, durationMs, opt);
    return 0;
}
