#pragma once

#include "cat_options.hpp"

#include <cstdint>
#include <istream>

class CharacterFormatter {
public:
    static void formatAndPrintChar(unsigned char c, bool showTabs, bool showNonPrinting);
};

class StreamProcessor {
private:
    const CatOptions& options;
    std::uint64_t currentLineNumber = 1;
    bool previousWasBlank = false;

    void printLineNumber();

public:
    explicit StreamProcessor(const CatOptions& opts);
    void processRaw(std::istream& in);
    void processFormatted(std::istream& in);
};
