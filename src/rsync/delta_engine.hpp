#ifndef DELTA_ENGINE_HPP
#define DELTA_ENGINE_HPP

#include "rsync.hpp"

class Adler32RollingChecksum {
private:
    static constexpr uint32_t MOD_ADLER = 65521;
    uint32_t a{1};
    uint32_t b{0};

public:
    Adler32RollingChecksum() = default;

    void compute(const uint8_t* data, size_t len);
    void roll(uint8_t outChar, uint8_t inChar, size_t blockLen);
    [[nodiscard]] uint32_t getValue() const;
};

class Fnv1a64StrongHash {
public:
    static uint64_t compute(const uint8_t* data, size_t len);
    static uint64_t computeFile(const fs::path& path);
};

struct BlockSignature {
    size_t index{0};
    uint32_t weakHash{0};
    uint64_t strongHash{0};
};

class DeltaEngine {
private:
    size_t blockSize;

public:
    explicit DeltaEngine(size_t chunkSize) : blockSize(chunkSize) {}

    std::vector<BlockSignature> generateSignatures(const fs::path& filePath) const;
    bool applyDeltaSync(const fs::path& src, const fs::path& dst, SyncStatistics& stats, bool dryRun);
};

#endif // DELTA_ENGINE_HPP
