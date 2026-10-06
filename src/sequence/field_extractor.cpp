#include "field_extractor.hpp"

std::string_view FieldExtractor::extractKey(const std::string& line, const KeyDefinition& key, char delimiter) {
    std::vector<std::pair<size_t, size_t>> fields;
    size_t n = line.size();

    if (delimiter == '\0') {
        size_t i = 0;
        while (i < n) {
            while (i < n && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
            if (i >= n) break;
            size_t start = i;
            while (i < n && !std::isspace(static_cast<unsigned char>(line[i]))) ++i;
            fields.emplace_back(start, i - start);
        }
    } else {
        size_t start = 0;
        for (size_t i = 0; i <= n; ++i) {
            if (i == n || line[i] == delimiter) {
                fields.emplace_back(start, i - start);
                start = i + 1;
            }
        }
    }

    if (fields.empty()) return std::string_view(line.data(), 0);

    int sfIdx = key.startField - 1;
    if (sfIdx >= static_cast<int>(fields.size())) return std::string_view(line.data(), 0);
    size_t charStart = fields[sfIdx].first + std::max(0, key.startChar - 1);
    if (charStart >= line.size()) return std::string_view(line.data(), 0);

    size_t charEnd = line.size();
    if (key.endField > 0) {
        int efIdx = key.endField - 1;
        if (efIdx < static_cast<int>(fields.size())) {
            if (key.endChar > 0) {
                charEnd = fields[efIdx].first + key.endChar;
            } else {
                charEnd = fields[efIdx].first + fields[efIdx].second;
            }
        }
    }

    if (charStart >= charEnd) return std::string_view(line.data(), 0);
    return std::string_view(line.data() + charStart, std::min(charEnd, line.size()) - charStart);
}
