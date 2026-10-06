#include "checksum_verifier.hpp"

bool Sha256ChecksumVerifier::parseLine(const std::string& line, std::string& expectedHash, bool& isBinary, std::string& filename) {
    if (line.length() < 66) return false;

    expectedHash = line.substr(0, 64);
    for (char c : expectedHash) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }

    size_t idx = 64;
    if (line[idx] != ' ' && line[idx] != '\t') return false;

    idx++;
    if (idx >= line.length()) return false;

    if (line[idx] == '*') {
        isBinary = true;
        idx++;
    } else if (line[idx] == ' ') {
        isBinary = false;
        idx++;
    } else {
        isBinary = false;
    }

    if (idx >= line.length()) return false;
    filename = line.substr(idx);

    if (!filename.empty() && filename.back() == '\r') {
        filename.pop_back();
    }

    return true;
}
