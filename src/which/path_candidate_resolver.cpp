#include "path_candidate_resolver.hpp"

std::string PathCandidateResolver::toLower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return str;
    }

std::vector<std::string> PathCandidateResolver::split(const std::string& str, char delim) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        while (std::getline(ss, token, delim)) {
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        return tokens;
    }

std::string PathCandidateResolver::trimQuotes(std::string str) {
        if (str.length() >= 2 && str.front() == '"' && str.back() == '"') {
            return str.substr(1, str.length() - 2);
        }
        return str;
    }

bool PathCandidateResolver::isExecutable(const fs::path& p) {
        std::error_code ec;
        return fs::exists(p, ec) && fs::is_regular_file(p, ec);
    }

std::vector<fs::path> PathCandidateResolver::getCandidates(const fs::path& basePath, const std::vector<std::string>& pathexts) {
        std::vector<fs::path> candidates;
        candidates.push_back(basePath);

        std::string ext = toLower(basePath.extension().string());
        bool hasExt = false;
        for (const auto& pe : pathexts) {
            if (ext == pe) {
                hasExt = true;
                break;
            }
        }

        if (!hasExt) {
            for (const auto& pe : pathexts) {
                fs::path p = basePath;
                p += pe;
                candidates.push_back(p);
            }
        }
        return candidates;
    }
