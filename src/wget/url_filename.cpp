#include "url_filename.hpp"

std::string GetFilenameFromUrl(const std::string& url) {
    size_t paramPos = url.find_first_of("?#");
    std::string cleanUrl = (paramPos != std::string::npos) ? url.substr(0, paramPos) : url;

    // Strip trailing slashes
    while (cleanUrl.length() > 8 && (cleanUrl.back() == '/' || cleanUrl.back() == '\\')) {
        cleanUrl.pop_back();
    }

    size_t lastSlash = cleanUrl.find_last_of("/\\");
    if (lastSlash != std::string::npos && lastSlash > 7 && lastSlash < cleanUrl.length() - 1) {
        std::string name = cleanUrl.substr(lastSlash + 1);
        // Replace invalid Windows filename characters
        for (char& c : name) {
            if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
                c = '_';
            }
        }
        if (!name.empty()) return name;
    }
    return "downloaded_file.bin";
}
