/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "entab_engine.hpp"

#include <array>
#include <limits>
#include <utility>

EntabEngine::EntabEngine(const EntabOptions& options) : options_(options) {}

bool EntabEngine::GetNextTabStop(std::size_t column, std::size_t& stop,
                                 bool& hasStop) const {
    if (!options_.tabStops.empty()) {
        for (std::size_t configuredStop : options_.tabStops) {
            if (configuredStop > column) {
                stop = configuredStop;
                hasStop = true;
                return true;
            }
        }
        hasStop = false;
        return true;
    }

    const std::size_t width = options_.tabWidth;
    const std::size_t remainder = column % width;
    const std::size_t advance = width - remainder;
    if (advance > (std::numeric_limits<std::size_t>::max)() - column) {
        hasStop = false;
        return true;
    }
    stop = column + advance;
    hasStop = true;
    return true;
}

bool EntabEngine::WriteCharacter(unsigned char value, EntabReporter& reporter,
                                 std::string& error) const {
    if (!reporter.WriteByte(value)) {
        error = "output write failed.";
        return false;
    }
    return true;
}

bool EntabEngine::FlushPendingBlanks(std::size_t startColumn,
                                     std::size_t endColumn, bool hadTab,
                                     EntabReporter& reporter,
                                     std::string& error) const {
    std::size_t column = startColumn;
    while (column < endColumn) {
        std::size_t nextStop = 0;
        bool hasStop = false;
        if (!GetNextTabStop(column, nextStop, hasStop)) {
            error = "could not calculate the next tab stop.";
            return false;
        }
        if (!hasStop || nextStop > endColumn) {
            while (column < endColumn) {
                if (!WriteCharacter(' ', reporter, error)) return false;
                ++column;
            }
            return true;
        }

        const std::size_t width = nextStop - column;
        if (width > 1 || hadTab) {
            if (!WriteCharacter('\t', reporter, error)) return false;
            column = nextStop;
        } else {
            if (!WriteCharacter(' ', reporter, error)) return false;
            ++column;
        }
    }
    return true;
}

bool EntabEngine::Convert(std::istream& input, EntabReporter& reporter,
                          std::string& error) const {
    std::array<char, EntabOptions::IO_BUFFER_SIZE> buffer{};
    std::size_t column = 0;
    std::size_t pendingStartColumn = 0;
    bool inInitialWhitespace = true;
    bool inPendingRun = false;
    bool pendingHadTab = false;

    auto advanceColumn = [&column, &error]() {
        if (column == (std::numeric_limits<std::size_t>::max)()) {
            error = "input line exceeds the supported column range.";
            return false;
        }
        ++column;
        return true;
    };

    for (;;) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize readCount = input.gcount();
        for (std::streamsize index = 0; index < readCount; ++index) {
            const unsigned char character =
                static_cast<unsigned char>(buffer[static_cast<std::size_t>(index)]);
            if (character == ' ' || character == '\t') {
                const bool shouldConvert =
                    options_.convertAll || inInitialWhitespace;
                if (shouldConvert) {
                    if (!inPendingRun) {
                        inPendingRun = true;
                        pendingStartColumn = column;
                        pendingHadTab = false;
                    }
                    if (character == '\t') {
                        pendingHadTab = true;
                        std::size_t nextStop = 0;
                        bool hasStop = false;
                        if (!GetNextTabStop(column, nextStop, hasStop)) {
                            error = "could not calculate the next tab stop.";
                            return false;
                        }
                        if (hasStop) column = nextStop;
                        else if (!advanceColumn()) return false;
                    } else if (!advanceColumn()) {
                        return false;
                    }
                } else {
                    if (inPendingRun &&
                        !FlushPendingBlanks(pendingStartColumn, column,
                                            pendingHadTab, reporter, error)) {
                        return false;
                    }
                    inPendingRun = false;
                    if (!WriteCharacter(character, reporter, error)) {
                        return false;
                    }
                }
                continue;
            }

            if (inPendingRun &&
                !FlushPendingBlanks(pendingStartColumn, column,
                                    pendingHadTab, reporter, error)) {
                return false;
            }
            inPendingRun = false;
            pendingHadTab = false;
            if (!WriteCharacter(character, reporter, error)) return false;

            if (character == '\n' || character == '\r') {
                column = 0;
                inInitialWhitespace = true;
            } else if (character == '\b') {
                if (column > 0) --column;
                inInitialWhitespace = false;
            } else {
                if (!advanceColumn()) return false;
                inInitialWhitespace = false;
            }
        }

        if (input.bad()) {
            error = "input read failed.";
            return false;
        }
        if (input.eof()) break;
        if (input.fail()) {
            error = "input read failed.";
            return false;
        }
    }

    if (inPendingRun &&
        !FlushPendingBlanks(pendingStartColumn, column, pendingHadTab,
                            reporter, error)) {
        return false;
    }
    return true;
}
