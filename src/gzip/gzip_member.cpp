#include "gzip_member.hpp"

#include <array>
#include <fstream>
#include <limits>
#include <utility>

namespace {
class BitReader {
private:
    std::istream& m_input;
    uint64_t m_nextOffset;
    unsigned int m_bitIndex = 8;
    uint8_t m_currentByte = 0;

public:
    BitReader(std::istream& input, uint64_t offset)
        : m_input(input), m_nextOffset(offset) {}

    bool readBits(unsigned int count, uint32_t& value) {
        value = 0;
        if (count > 24) return false;
        for (unsigned int i = 0; i < count; ++i) {
            if (m_bitIndex == 8) {
                char next = 0;
                if (!m_input.get(next)) return false;
                m_currentByte = static_cast<uint8_t>(
                    static_cast<unsigned char>(next));
                ++m_nextOffset;
                m_bitIndex = 0;
            }
            value |= static_cast<uint32_t>((m_currentByte >> m_bitIndex) & 1u) << i;
            ++m_bitIndex;
        }
        return true;
    }

    void alignToByte() {
        if (m_bitIndex != 8) m_bitIndex = 8;
    }

    bool skipAligned(uint64_t count) {
        alignToByte();
        if (count > static_cast<uint64_t>(
                (std::numeric_limits<std::streamoff>::max)()) - m_nextOffset) {
            return false;
        }
        m_input.seekg(static_cast<std::streamoff>(count), std::ios::cur);
        if (!m_input) return false;
        m_nextOffset += count;
        return true;
    }

    uint64_t position() const {
        return m_nextOffset;
    }
};

class HuffmanTree {
private:
    std::array<std::vector<int>, 16> m_symbolsByCode;
    unsigned int m_maxBits = 0;

public:
    bool build(const std::vector<uint8_t>& lengths, bool allowEmpty) {
        std::array<unsigned int, 16> counts{};
        for (uint8_t length : lengths) {
            if (length > 15) return false;
            if (length != 0) ++counts[length];
        }

        m_maxBits = 0;
        for (unsigned int bitLength = 1; bitLength <= 15; ++bitLength) {
            if (counts[bitLength] != 0) m_maxBits = bitLength;
            m_symbolsByCode[bitLength].clear();
        }
        if (m_maxBits == 0) return allowEmpty;

        int remaining = 1;
        for (unsigned int bitLength = 1; bitLength <= 15; ++bitLength) {
            remaining = (remaining << 1) - static_cast<int>(counts[bitLength]);
            if (remaining < 0) return false;
        }

        std::array<uint32_t, 16> nextCode{};
        uint32_t code = 0;
        for (unsigned int bitLength = 1; bitLength <= 15; ++bitLength) {
            code = (code + counts[bitLength - 1]) << 1;
            nextCode[bitLength] = code;
            if (counts[bitLength] != 0) {
                m_symbolsByCode[bitLength].assign(
                    static_cast<size_t>(1u << bitLength), -1);
            }
        }

        for (size_t symbol = 0; symbol < lengths.size(); ++symbol) {
            const unsigned int bitLength = lengths[symbol];
            if (bitLength == 0) continue;
            const uint32_t canonical = nextCode[bitLength]++;
            m_symbolsByCode[bitLength][canonical] = static_cast<int>(symbol);
        }
        return true;
    }

    bool decode(BitReader& input, int& symbol) const {
        uint32_t code = 0;
        for (unsigned int bitLength = 1; bitLength <= m_maxBits; ++bitLength) {
            uint32_t bit = 0;
            if (!input.readBits(1, bit)) return false;
            code = (code << 1) | bit;
            if (!m_symbolsByCode[bitLength].empty() &&
                code < m_symbolsByCode[bitLength].size()) {
                const int candidate = m_symbolsByCode[bitLength][code];
                if (candidate >= 0) {
                    symbol = candidate;
                    return true;
                }
            }
        }
        return false;
    }
};

bool BuildFixedTrees(HuffmanTree& literals, HuffmanTree& distances) {
    std::vector<uint8_t> literalLengths(288);
    for (size_t symbol = 0; symbol <= 143; ++symbol) literalLengths[symbol] = 8;
    for (size_t symbol = 144; symbol <= 255; ++symbol) literalLengths[symbol] = 9;
    for (size_t symbol = 256; symbol <= 279; ++symbol) literalLengths[symbol] = 7;
    for (size_t symbol = 280; symbol <= 287; ++symbol) literalLengths[symbol] = 8;
    return literals.build(literalLengths, false) &&
           distances.build(std::vector<uint8_t>(32, 5), false);
}

bool BuildDynamicTrees(BitReader& input, HuffmanTree& literals,
                       HuffmanTree& distances) {
    uint32_t value = 0;
    if (!input.readBits(5, value)) return false;
    const size_t literalCount = value + 257;
    if (!input.readBits(5, value)) return false;
    const size_t distanceCount = value + 1;
    if (!input.readBits(4, value)) return false;
    const size_t codeLengthCount = value + 4;

    static constexpr std::array<unsigned int, 19> order = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };
    std::vector<uint8_t> codeLengths(19, 0);
    for (size_t i = 0; i < codeLengthCount; ++i) {
        if (!input.readBits(3, value)) return false;
        codeLengths[order[i]] = static_cast<uint8_t>(value);
    }

    HuffmanTree codeLengthTree;
    if (!codeLengthTree.build(codeLengths, false)) return false;
    std::vector<uint8_t> allLengths;
    allLengths.reserve(literalCount + distanceCount);
    const size_t required = literalCount + distanceCount;
    while (allLengths.size() < required) {
        int symbol = 0;
        if (!codeLengthTree.decode(input, symbol)) return false;
        if (symbol <= 15) {
            allLengths.push_back(static_cast<uint8_t>(symbol));
            continue;
        }

        size_t repeatCount = 0;
        uint8_t repeatedValue = 0;
        if (symbol == 16) {
            if (allLengths.empty() || !input.readBits(2, value)) return false;
            repeatCount = static_cast<size_t>(value) + 3;
            repeatedValue = allLengths.back();
        } else if (symbol == 17) {
            if (!input.readBits(3, value)) return false;
            repeatCount = static_cast<size_t>(value) + 3;
        } else if (symbol == 18) {
            if (!input.readBits(7, value)) return false;
            repeatCount = static_cast<size_t>(value) + 11;
        } else {
            return false;
        }
        if (repeatCount > required - allLengths.size()) return false;
        allLengths.insert(allLengths.end(), repeatCount, repeatedValue);
    }

    std::vector<uint8_t> literalLengths(
        allLengths.begin(), allLengths.begin() + static_cast<std::ptrdiff_t>(literalCount));
    std::vector<uint8_t> distanceLengths(
        allLengths.begin() + static_cast<std::ptrdiff_t>(literalCount), allLengths.end());
    if (literalLengths.size() <= 256 || literalLengths[256] == 0) return false;
    return literals.build(literalLengths, false) &&
           distances.build(distanceLengths, true);
}

bool SkipCompressedBlock(BitReader& input, const HuffmanTree& literals,
                         const HuffmanTree& distances) {
    static constexpr std::array<unsigned int, 29> lengthExtra = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
    };
    static constexpr std::array<unsigned int, 30> distanceExtra = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
        6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
    };

    for (;;) {
        int symbol = 0;
        if (!literals.decode(input, symbol)) return false;
        if (symbol < 256) continue;
        if (symbol == 256) return true;
        if (symbol < 257 || symbol > 285) return false;

        uint32_t ignored = 0;
        if (!input.readBits(lengthExtra[static_cast<size_t>(symbol - 257)], ignored)) {
            return false;
        }
        int distanceSymbol = 0;
        if (!distances.decode(input, distanceSymbol) ||
            distanceSymbol < 0 || distanceSymbol > 29) {
            return false;
        }
        if (!input.readBits(distanceExtra[static_cast<size_t>(distanceSymbol)],
                            ignored)) {
            return false;
        }
    }
}

bool ScanDeflate(BitReader& input, std::wstring& error) {
    for (;;) {
        uint32_t finalBlock = 0;
        uint32_t blockType = 0;
        if (!input.readBits(1, finalBlock) || !input.readBits(2, blockType)) {
            error = L"truncated DEFLATE block header";
            return false;
        }

        if (blockType == 0) {
            input.alignToByte();
            uint32_t length = 0;
            uint32_t complement = 0;
            if (!input.readBits(16, length) ||
                !input.readBits(16, complement) ||
                (length ^ complement) != 0xffffu ||
                !input.skipAligned(length)) {
                error = L"invalid or truncated stored DEFLATE block";
                return false;
            }
        } else if (blockType == 1 || blockType == 2) {
            HuffmanTree literals;
            HuffmanTree distances;
            const bool treesBuilt = blockType == 1
                ? BuildFixedTrees(literals, distances)
                : BuildDynamicTrees(input, literals, distances);
            if (!treesBuilt || !SkipCompressedBlock(input, literals, distances)) {
                error = L"invalid or truncated compressed DEFLATE block";
                return false;
            }
        } else {
            error = L"reserved DEFLATE block type";
            return false;
        }

        if (finalBlock != 0) {
            input.alignToByte();
            return true;
        }
    }
}

bool ReadByte(std::istream& input, uint8_t& value) {
    char character = 0;
    if (!input.get(character)) return false;
    value = static_cast<uint8_t>(static_cast<unsigned char>(character));
    return true;
}

uint32_t UpdateCrc32(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (unsigned int bit = 0; bit < 8; ++bit) {
        crc = (crc >> 1) ^ ((crc & 1u) != 0 ? 0xedb88320u : 0u);
    }
    return crc;
}

bool ReadLittleEndian16(std::istream& input, uint16_t& value,
                        uint32_t* headerCrc = nullptr) {
    uint8_t low = 0;
    uint8_t high = 0;
    if (!ReadByte(input, low) || !ReadByte(input, high)) return false;
    if (headerCrc != nullptr) {
        *headerCrc = UpdateCrc32(*headerCrc, low);
        *headerCrc = UpdateCrc32(*headerCrc, high);
    }
    value = static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8));
    return true;
}

bool ReadHeaderBytes(std::istream& input, uint64_t count, uint64_t fileSize,
                     uint32_t& headerCrc) {
    const std::streampos current = input.tellg();
    if (current < 0) return false;
    const uint64_t offset = static_cast<uint64_t>(current);
    if (offset > fileSize || count > fileSize - offset) return false;
    for (uint64_t i = 0; i < count; ++i) {
        uint8_t byte = 0;
        if (!ReadByte(input, byte)) return false;
        headerCrc = UpdateCrc32(headerCrc, byte);
    }
    return true;
}

bool ReadZeroTerminatedHeaderField(std::istream& input, uint64_t fileSize,
                                   uint32_t& headerCrc) {
    for (;;) {
        const std::streampos current = input.tellg();
        if (current < 0 || static_cast<uint64_t>(current) >= fileSize) return false;
        uint8_t byte = 0;
        if (!ReadByte(input, byte)) return false;
        headerCrc = UpdateCrc32(headerCrc, byte);
        if (byte == 0) return true;
    }
}
}

bool FindGzipMembers(const std::filesystem::path& path,
                     std::vector<GzipMember>& members, std::wstring& error) {
    members.clear();
    error.clear();
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = L"cannot open gzip input";
        return false;
    }
    input.seekg(0, std::ios::end);
    const std::streampos endPosition = input.tellg();
    if (endPosition <= 0) {
        error = L"gzip input is empty or cannot be measured";
        return false;
    }
    const uint64_t fileSize = static_cast<uint64_t>(endPosition);
    input.seekg(0, std::ios::beg);
    if (!input) {
        error = L"cannot seek gzip input";
        return false;
    }

    while (static_cast<uint64_t>(input.tellg()) < fileSize) {
        if (members.size() >= 1000000) {
            error = L"gzip input contains too many concatenated members";
            return false;
        }
        const std::streampos memberStartPosition = input.tellg();
        if (memberStartPosition < 0) {
            error = L"cannot locate gzip member";
            return false;
        }
        const uint64_t memberStart = static_cast<uint64_t>(memberStartPosition);
        if (fileSize - memberStart < 10) {
            error = L"truncated gzip header";
            return false;
        }

        std::array<uint8_t, 10> header{};
        for (uint8_t& byte : header) {
            if (!ReadByte(input, byte)) {
                error = L"truncated gzip header";
                return false;
            }
        }
        if (header[0] != 0x1f || header[1] != 0x8b || header[2] != 8 ||
            (header[3] & 0xe0) != 0) {
            error = L"invalid gzip header";
            return false;
        }

        uint32_t headerCrc = 0xffffffffu;
        for (uint8_t byte : header) headerCrc = UpdateCrc32(headerCrc, byte);
        const uint8_t flags = header[3];
        if ((flags & 0x04) != 0) {
            uint16_t extraLength = 0;
            if (!ReadLittleEndian16(input, extraLength, &headerCrc) ||
                !ReadHeaderBytes(input, extraLength, fileSize, headerCrc)) {
                error = L"truncated gzip extra field";
                return false;
            }
        }
        if ((flags & 0x08) != 0 &&
            !ReadZeroTerminatedHeaderField(input, fileSize, headerCrc)) {
            error = L"truncated gzip original-name field";
            return false;
        }
        if ((flags & 0x10) != 0 &&
            !ReadZeroTerminatedHeaderField(input, fileSize, headerCrc)) {
            error = L"truncated gzip comment field";
            return false;
        }
        if ((flags & 0x02) != 0) {
            uint16_t actualHeaderCrc = 0;
            if (!ReadLittleEndian16(input, actualHeaderCrc)) {
                error = L"truncated gzip header checksum";
                return false;
            }
            const uint16_t expectedHeaderCrc = static_cast<uint16_t>(
                headerCrc ^ 0xffffffffu);
            if (actualHeaderCrc != expectedHeaderCrc) {
                error = L"gzip header checksum mismatch";
                return false;
            }
        }

        const std::streampos deflatePosition = input.tellg();
        if (deflatePosition < 0) {
            error = L"cannot locate gzip DEFLATE data";
            return false;
        }
        BitReader bits(input, static_cast<uint64_t>(deflatePosition));
        if (!ScanDeflate(bits, error)) return false;
        const uint64_t trailerOffset = bits.position();
        if (trailerOffset > fileSize || fileSize - trailerOffset < 8) {
            error = L"truncated gzip trailer";
            return false;
        }
        input.clear();
        input.seekg(static_cast<std::streamoff>(trailerOffset), std::ios::beg);
        if (!input) {
            error = L"cannot seek to gzip trailer";
            return false;
        }
        std::array<uint8_t, 8> trailer{};
        for (uint8_t& byte : trailer) {
            if (!ReadByte(input, byte)) {
                error = L"truncated gzip trailer";
                return false;
            }
        }
        const std::streampos memberEndPosition = input.tellg();
        if (memberEndPosition < 0) {
            error = L"cannot locate end of gzip member";
            return false;
        }
        const uint64_t memberEnd = static_cast<uint64_t>(memberEndPosition);
        members.push_back({memberStart, memberEnd - memberStart});
    }

    if (members.empty()) {
        error = L"gzip input contains no members";
        return false;
    }
    return true;
}
