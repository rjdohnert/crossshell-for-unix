#include "line_comparator.hpp"
#include "line_extractor.hpp"

LineComparator::LineComparator(bool iCase, size_t fields, size_t chars, size_t maxChars)
        : ignoreCase(iCase), skipFields(fields), skipChars(chars), checkChars(maxChars) {}

[[nodiscard]] bool LineComparator::areEqual(std::string_view lineA, std::string_view lineB) const {
        std::string_view keyA = LineExtractor::extractKey(lineA, skipFields, skipChars, checkChars);
        std::string_view keyB = LineExtractor::extractKey(lineB, skipFields, skipChars, checkChars);

        if (keyA.length() != keyB.length()) return false;

        if (ignoreCase) {
            return std::equal(keyA.begin(), keyA.end(), keyB.begin(), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
        }

        return keyA == keyB;
    }
