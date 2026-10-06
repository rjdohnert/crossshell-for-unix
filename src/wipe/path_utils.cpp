#include "path_utils.hpp"

std::mt19937_64& PathUtils::GetRng() {
        static std::mt19937_64 rng(std::random_device{}());
        return rng;
    }

std::string PathUtils::RandomString(size_t length) {
        static const char charset[] =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);
        std::string str;
        str.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            str += charset[dist(GetRng())];
        }
        return str;
    }

std::string PathUtils::NormalizePath(const std::string& path) {
        std::string normalized = path;
        std::replace(normalized.begin(), normalized.end(), '/', '\\');
        while (normalized.size() > 1 && normalized.back() == '\\') {
            normalized.pop_back();
        }
        if (normalized.size() == 2 && normalized[1] == ':') {
            normalized.push_back('\\');
        }
        return normalized;
    }

std::string PathUtils::JoinPath(const std::string& base, const std::string& child) {
        std::string result = NormalizePath(base);
        if (result.empty()) {
            return child;
        }
        if (result.back() == '\\') {
            return result + child;
        }
        return result + "\\" + child;
    }

std::string PathUtils::GetParentDir(const std::string& path) {
        size_t pos = path.find_last_of("/\\");
        if (pos == std::string::npos) return "";
        return path.substr(0, pos + 1);
    }

std::string PathUtils::GetRandomFilePath(const std::string& original_path) {
        std::string parent = GetParentDir(original_path);
        return parent + RandomString(12) + ".tmp";
    }

std::mt19937_64& PathUtils::Rng() {
        return GetRng();
    }
