#ifndef SHA256_DIGEST_HPP
#define SHA256_DIGEST_HPP

#include "sha256sum.hpp"

class SHA256DigestEngine {
private:
    uint32_t m_state[8];
    uint64_t m_count;
    uint8_t m_buffer[64];
    size_t m_buffer_len;

    static uint32_t rotr(uint32_t x, uint32_t n);
    static uint32_t choose(uint32_t e, uint32_t f, uint32_t g);
    static uint32_t majority(uint32_t a, uint32_t b, uint32_t c);
    static uint32_t sig0(uint32_t x);
    static uint32_t sig1(uint32_t x);
    static uint32_t sub0(uint32_t x);
    static uint32_t sub1(uint32_t x);
    void transform(const uint8_t chunk[64]);

public:
    SHA256DigestEngine();
    void init();
    void update(const uint8_t* data, size_t len);
    std::string finalize();
};

#endif // SHA256_DIGEST_HPP
