#pragma once

#include "rpmbuild.hpp"

class CpioCompiler {
public:
    static void appendFile(std::ostream& out, const std::string& archivePath, const fs::path& sourceFile, uint32_t mode);

    static void appendTrailer(std::ostream& out);
};
