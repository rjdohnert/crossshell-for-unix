#include "gsar_engine.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <sstream>

namespace {
constexpr size_t kChunkSize = 64 * 1024;

uint8_t FoldAscii(uint8_t value) {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<uint8_t>(value + ('a' - 'A'));
    }
    return value;
}

void PrintHexMatch(std::ostream& output, uint64_t offset,
                   const uint8_t* bytes, size_t length) {
    std::ostringstream line;
    line << "Match at 0x" << std::hex << offset << ": ";
    for (size_t i = 0; i < length; ++i) {
        line << std::setw(2) << std::setfill('0') << static_cast<unsigned>(bytes[i]) << ' ';
    }
    line << " | ";
    for (size_t i = 0; i < length; ++i) {
        const unsigned char value = bytes[i];
        line << ((value >= 0x20 && value <= 0x7e) ? static_cast<char>(value) : '.');
    }
    line << '\n';
    output << line.str();
}
}

SearchEngine::SearchEngine(const Options& options) : m_options(options) {}

bool SearchEngine::matchesAt(const uint8_t* buffer, size_t position,
                             size_t bufferSize) const {
    const size_t patternSize = m_options.searchPattern.size();
    if (position > bufferSize || patternSize > bufferSize - position) return false;

    for (size_t i = 0; i < patternSize; ++i) {
        const uint8_t actual = buffer[position + i];
        const uint8_t expected = m_options.searchPattern[i];
        if (m_options.caseInsensitive) {
            if (FoldAscii(actual) != FoldAscii(expected)) return false;
        } else if (actual != expected) {
            return false;
        }
    }
    return true;
}

SearchResult SearchEngine::processStream(std::istream& input, std::ostream* output,
                                         std::ostream& diagnostics) const {
    SearchResult result;
    const size_t patternSize = m_options.searchPattern.size();
    if (patternSize == 0) {
        result.succeeded = false;
        result.error = "search pattern must not be empty";
        return result;
    }

    std::vector<uint8_t> buffer;
    std::array<char, kChunkSize> readChunk{};
    uint64_t streamOffset = 0;

    auto writeBytes = [&](const uint8_t* bytes, size_t length) {
        if (output == nullptr || m_options.countOnly || length == 0) return true;
        output->write(reinterpret_cast<const char*>(bytes),
                      static_cast<std::streamsize>(length));
        if (!*output) {
            result.succeeded = false;
            result.error = "failed while writing transformed data";
            return false;
        }
        if (length > std::numeric_limits<uint64_t>::max() - result.bytesWritten) {
            result.succeeded = false;
            result.error = "output byte count overflow";
            return false;
        }
        result.bytesWritten += static_cast<uint64_t>(length);
        return true;
    };

    for (;;) {
        input.read(readChunk.data(), static_cast<std::streamsize>(readChunk.size()));
        const std::streamsize bytesRead = input.gcount();

        if (bytesRead > 0) {
            const uint64_t count = static_cast<uint64_t>(bytesRead);
            if (count > std::numeric_limits<uint64_t>::max() - result.bytesRead) {
                result.succeeded = false;
                result.error = "input byte count overflow";
                return result;
            }
            result.bytesRead += count;
            buffer.insert(buffer.end(), readChunk.begin(), readChunk.begin() + bytesRead);

            const size_t processLimit = buffer.size() >= patternSize
                ? buffer.size() - patternSize + 1
                : 0;
            size_t position = 0;
            while (position < processLimit) {
                if (matchesAt(buffer.data(), position, buffer.size())) {
                    if (result.matchCount == std::numeric_limits<uint64_t>::max()) {
                        result.succeeded = false;
                        result.error = "match count overflow";
                        return result;
                    }
                    const uint64_t matchOffset = streamOffset + position;
                    ++result.matchCount;

                    if (m_options.showOffsets) {
                        diagnostics << "Match at offset: 0x" << std::hex << matchOffset
                                    << std::dec << '\n';
                    }
                    if (m_options.displayHex) {
                        PrintHexMatch(diagnostics, matchOffset, buffer.data() + position,
                                      patternSize);
                    }
                    if (!diagnostics) {
                        result.succeeded = false;
                        result.error = "failed while writing match diagnostics";
                        return result;
                    }

                    const uint8_t* replacement = m_options.hasReplace
                        ? m_options.replacePattern.data()
                        : buffer.data() + position;
                    const size_t replacementSize = m_options.hasReplace
                        ? m_options.replacePattern.size()
                        : patternSize;
                    if (!writeBytes(replacement, replacementSize)) return result;
                    position += patternSize;
                } else {
                    if (!writeBytes(buffer.data() + position, 1)) return result;
                    ++position;
                }
            }

            if (position > std::numeric_limits<uint64_t>::max() - streamOffset) {
                result.succeeded = false;
                result.error = "input offset overflow";
                return result;
            }
            streamOffset += position;
            buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(position));
        }

        if (input.bad()) {
            result.succeeded = false;
            result.error = "failed while reading input data";
            return result;
        }
        if (input.eof()) break;
        if (input.fail()) {
            result.succeeded = false;
            result.error = "input stream stopped before end-of-file";
            return result;
        }
        if (bytesRead == 0) {
            result.succeeded = false;
            result.error = "input stream made no progress";
            return result;
        }
    }

    if (!buffer.empty() && !writeBytes(buffer.data(), buffer.size())) return result;
    if (output != nullptr && !m_options.countOnly) {
        output->flush();
        if (!*output) {
            result.succeeded = false;
            result.error = "failed while flushing transformed data";
        }
    }
    return result;
}
