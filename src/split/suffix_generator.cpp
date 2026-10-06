#include "suffix_generator.hpp"

std::string SuffixGenerator::generate(uint64_t index, int len, SuffixType type) {
        std::string s(len, ' ');
        uint64_t base = (type == SuffixType::Alpha) ? 26 : ((type == SuffixType::Numeric) ? 10 : 16);

        for (int i = len - 1; i >= 0; --i) {
            uint64_t rem = index % base;
            if (type == SuffixType::Alpha) {
                s[i] = static_cast<char>('a' + rem);
            } else if (type == SuffixType::Numeric) {
                s[i] = static_cast<char>('0' + rem);
            } else {
                s[i] = (rem < 10) ? static_cast<char>('0' + rem) : static_cast<char>('a' + (rem - 10));
            }
            index /= base;
        }
        return s;
    }
