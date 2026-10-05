// ============================================================================
// Core Search & Replace Engine
// ============================================================================
class SearchEngine {
private:
    Options options;
    static constexpr size_t CHUNK_SIZE = 64 * 1024; // 64 KB buffer

    bool matchesAt(const uint8_t* buffer, size_t pos, size_t bufferSize) const {
        if (pos + options.searchPattern.size() > bufferSize) return false;
        
        for (size_t i = 0; i < options.searchPattern.size(); ++i) {
            uint8_t a = buffer[pos + i];
            uint8_t b = options.searchPattern[i];
            if (options.caseInsensitive) {
                if (std::tolower(static_cast<unsigned char>(a)) != std::tolower(static_cast<unsigned char>(b))) {
                    return false;
                }
            } else {
                if (a != b) return false;
            }
        }
        return true;
    }

    void displayHexContext(uint64_t offset, const uint8_t* data, size_t length) const {
        std::cout << "[Match at 0x" << std::hex << std::setw(8) << std::setfill('0') << offset << "] ";
        for (size_t i = 0; i < length; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
        }
        std::cout << " | ";
        for (size_t i = 0; i < length; ++i) {
            char c = static_cast<char>(data[i]);
            std::cout << (std::isprint(static_cast<unsigned char>(c)) ? c : '.');
        }
        std::cout << std::dec << "\n";
    }

public:
    explicit SearchEngine(Options opt) : options(std::move(opt)) {}

    struct Result {
        uint64_t matchCount = 0;
        uint64_t bytesRead = 0;
        uint64_t bytesWritten = 0;
    };

    Result processStream(std::istream& in, std::ostream* out) {
        Result res;
        const size_t patSize = options.searchPattern.size();
        
        if (patSize == 0) return res;

        std::vector<uint8_t> buffer;
        buffer.reserve(CHUNK_SIZE + patSize);
        std::vector<uint8_t> readChunk(CHUNK_SIZE);

        uint64_t streamOffset = 0;

        while (in) {
            in.read(reinterpret_cast<char*>(readChunk.data()), CHUNK_SIZE);
            std::streamsize bytesJustRead = in.gcount();
            if (bytesJustRead <= 0) break;

            res.bytesRead += bytesJustRead;
            buffer.insert(buffer.end(), readChunk.begin(), readChunk.begin() + bytesJustRead);

            size_t i = 0;
            // Process buffer while we have enough room to check pattern
            while (i + patSize <= buffer.size()) {
                if (matchesAt(buffer.data(), i, buffer.size())) {
                    res.matchCount++;
                    uint64_t matchAbsoluteOffset = streamOffset + i;

                    if (options.showOffsets) {
                        std::cout << "Match at offset: 0x" << std::hex << matchAbsoluteOffset << std::dec << "\n";
                    }
                    if (options.displayHex) {
                        displayHexContext(matchAbsoluteOffset, buffer.data() + i, patSize);
                    }

                    if (out && options.hasReplace && !options.countOnly) {
                        if (!options.replacePattern.empty()) {
                            out->write(reinterpret_cast<const char*>(options.replacePattern.data()), options.replacePattern.size());
                            res.bytesWritten += options.replacePattern.size();
                        }
                    }
                    i += patSize;
                } else {
                    if (out && !options.countOnly) {
                        out->put(static_cast<char>(buffer[i]));
                        res.bytesWritten++;
                    }
                    i++;
                }
            }

            streamOffset += i;
            // Keep remaining tail of the buffer for the next sliding overlap
            buffer.erase(buffer.begin(), buffer.begin() + i);
        }

        // Flush remaining buffer bytes that could not form a full match
        if (out && !options.countOnly && !buffer.empty()) {
            out->write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
            res.bytesWritten += buffer.size();
        }

        return res;
    }
};

