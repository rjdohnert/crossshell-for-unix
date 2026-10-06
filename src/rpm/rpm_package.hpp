#pragma once

#include "rpm_metadata.hpp"
#include "rpm.hpp"

class RpmPackage {
public:
    RpmMetadata meta;
    std::streampos payloadOffset = 0;
    fs::path sourcePath;

    bool parse(const fs::path& rpmPath);

private:
    bool parseHeader(std::ifstream& file, bool isSignature);
};
