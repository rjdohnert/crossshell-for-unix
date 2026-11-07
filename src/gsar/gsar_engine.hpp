#pragma once

#include "gsar_types.hpp"

#include <istream>
#include <ostream>

struct SearchResult {
    uint64_t matchCount = 0;
    uint64_t bytesRead = 0;
    uint64_t bytesWritten = 0;
    bool succeeded = true;
    std::string error;
};

class SearchEngine {
private:
    const Options& m_options;

    bool matchesAt(const uint8_t* buffer, size_t position, size_t bufferSize) const;

public:
    explicit SearchEngine(const Options& options);
    SearchResult processStream(std::istream& input, std::ostream* output,
                               std::ostream& diagnostics) const;
};
