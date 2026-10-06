#include "location_searcher.hpp"

std::vector<std::wstring> LocationSearcher::searchCategory(const std::wstring& target,
                                                    const std::vector<std::wstring>& dirs,
                                                    const std::vector<std::wstring>& exts) {
        std::vector<std::wstring> matches;
        std::set<std::wstring> visited;

        for (const auto& dir : dirs) {
            if (dir.empty()) continue;
            for (const auto& ext : exts) {
                fs::path candidate = fs::path(dir) / (target + ext);
                std::error_code ec;
                if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
                    std::wstring fullPath = fs::absolute(candidate, ec).wstring();
                    std::wstring lowerKey = fullPath;
                    std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
                    if (visited.find(lowerKey) == visited.end()) {
                        visited.insert(lowerKey);
                        matches.push_back(fullPath);
                    }
                }
            }
        }
        return matches;
    }
