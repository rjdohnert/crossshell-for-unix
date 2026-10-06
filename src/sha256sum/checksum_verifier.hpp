#ifndef CHECKSUM_VERIFIER_HPP
#define CHECKSUM_VERIFIER_HPP

#include "sha256sum.hpp"

class Sha256ChecksumVerifier {
public:
    static bool parseLine(const std::string& line, std::string& expectedHash, bool& isBinary, std::string& filename);
};

#endif // CHECKSUM_VERIFIER_HPP
