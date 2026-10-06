#pragma once

#include "uniq.hpp"

class LineComparator {
private:
    bool ignoreCase;
    size_t skipFields;
    size_t skipChars;
    size_t checkChars;

public:
    LineComparator(bool iCase, size_t fields, size_t chars, size_t maxChars);

    [[nodiscard]] bool areEqual(std::string_view lineA, std::string_view lineB) const;
};
