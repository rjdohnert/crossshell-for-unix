#include "line_extractor.hpp"

std::string_view LineExtractor::extractKey(std::string_view line, size_t skipFields, size_t skipChars, size_t checkChars) {
        size_t offset = 0;

        for (size_t f = 0; f < skipFields && offset < line.length(); ++f) {
            while (offset < line.length() && std::isspace(static_cast<unsigned char>(line[offset]))) {
                offset++;
            }
            while (offset < line.length() && !std::isspace(static_cast<unsigned char>(line[offset]))) {
                offset++;
            }
        }

        offset = (std::min)(line.length(), offset + skipChars);
        std::string_view key = line.substr(offset);

        if (checkChars > 0 && checkChars < key.length()) {
            key = key.substr(0, checkChars);
        }

        return key;
    }
