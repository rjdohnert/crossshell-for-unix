#include "http_url.hpp"

bool IsHttpUrl(const std::string& url) {
    std::string lower = url;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lower.rfind("http://", 0) == 0 || lower.rfind("https://", 0) == 0;
}
