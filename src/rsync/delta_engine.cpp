#include "delta_engine.hpp"

void Adler32RollingChecksum::compute(const uint8_t* data, size_t len) {
    a = 1;
    b = 0;
    for (size_t i = 0; i < len; ++i) {
        a = (a + data[i]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
}

void Adler32RollingChecksum::roll(uint8_t outChar, uint8_t inChar, size_t blockLen) {
    a = (a + MOD_ADLER + inChar - outChar) % MOD_ADLER;
    b = (b + MOD_ADLER + a - (static_cast<uint32_t>(blockLen) * outChar) - 1) % MOD_ADLER;
}

uint32_t Adler32RollingChecksum::getValue() const {
    return (b << 16) | a;
}

uint64_t Fnv1a64StrongHash::compute(const uint8_t* data, size_t len) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

uint64_t Fnv1a64StrongHash::computeFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return 0;

    uint64_t hash = 0xcbf29ce484222325ULL;
    std::vector<uint8_t> buffer(65536);
    while (file.read(reinterpret_cast<char*>(buffer.data()), buffer.size()) || file.gcount() > 0) {
        size_t bytes = static_cast<size_t>(file.gcount());
        for (size_t i = 0; i < bytes; ++i) {
            hash ^= static_cast<uint64_t>(buffer[i]);
            hash *= 0x100000001b3ULL;
        }
    }
    return hash;
}

std::vector<BlockSignature> DeltaEngine::generateSignatures(const fs::path& filePath) const {
    std::vector<BlockSignature> signatures;
    std::ifstream file(filePath, std::ios::binary);
    if (!file) return signatures;

    std::vector<uint8_t> buffer(blockSize);
    size_t blockIdx = 0;

    while (file.read(reinterpret_cast<char*>(buffer.data()), blockSize) || file.gcount() > 0) {
        size_t bytesRead = static_cast<size_t>(file.gcount());
        Adler32RollingChecksum adler;
        adler.compute(buffer.data(), bytesRead);
        uint64_t strong = Fnv1a64StrongHash::compute(buffer.data(), bytesRead);

        signatures.push_back({blockIdx++, adler.getValue(), strong});
    }
    return signatures;
}

bool DeltaEngine::applyDeltaSync(const fs::path& src, const fs::path& dst, SyncStatistics& stats, bool dryRun) {
    if (!fs::exists(dst)) {
        uint64_t srcSize = fs::file_size(src);
        stats.bytesTransferred += srcSize;
        stats.totalBytes += srcSize;
        stats.filesTransferred++;

        if (!dryRun) {
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
        }
        return true;
    }

    uint64_t srcSize = fs::file_size(src);
    stats.totalBytes += srcSize;

    auto signatures = generateSignatures(dst);
    std::unordered_map<uint32_t, std::vector<BlockSignature>> sigLookup;
    for (const auto& sig : signatures) {
        sigLookup[sig.weakHash].push_back(sig);
    }

    std::ifstream srcFile(src, std::ios::binary);
    if (!srcFile) return false;

    fs::path tempTarget = dst.string() + ".rsync_tmp";
    std::ofstream outFile;
    if (!dryRun) {
        outFile.open(tempTarget, std::ios::binary);
        if (!outFile) return false;
    }

    std::ifstream dstReader(dst, std::ios::binary);
    std::vector<uint8_t> window(blockSize);
    uint64_t matchedBytes = 0;
    uint64_t literalBytes = 0;

    while (srcFile.read(reinterpret_cast<char*>(window.data()), blockSize) || srcFile.gcount() > 0) {
        size_t bytesRead = static_cast<size_t>(srcFile.gcount());
        Adler32RollingChecksum adler;
        adler.compute(window.data(), bytesRead);
        uint32_t weak = adler.getValue();

        bool blockMatched = false;
        if (sigLookup.find(weak) != sigLookup.end()) {
            uint64_t strong = Fnv1a64StrongHash::compute(window.data(), bytesRead);
            for (const auto& matchSig : sigLookup[weak]) {
                if (matchSig.strongHash == strong) {
                    blockMatched = true;
                    matchedBytes += bytesRead;

                    if (!dryRun) {
                        dstReader.seekg(matchSig.index * blockSize);
                        std::vector<uint8_t> dstBlock(bytesRead);
                        dstReader.read(reinterpret_cast<char*>(dstBlock.data()), bytesRead);
                        outFile.write(reinterpret_cast<char*>(dstBlock.data()), bytesRead);
                    }
                    break;
                }
            }
        }

        if (!blockMatched) {
            literalBytes += bytesRead;
            if (!dryRun) {
                outFile.write(reinterpret_cast<char*>(window.data()), bytesRead);
            }
        }
    }

    if (!dryRun) {
        outFile.close();
        dstReader.close();
        srcFile.close();
        fs::rename(tempTarget, dst);
    }

    stats.bytesTransferred += literalBytes;
    stats.deltaBytesSaved += matchedBytes;
    stats.filesTransferred++;
    return true;
}
