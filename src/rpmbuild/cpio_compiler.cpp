#include "cpio_compiler.hpp"

void CpioCompiler::appendFile(std::ostream& out, const std::string& archivePath, const fs::path& sourceFile, uint32_t mode) {
        uint32_t filesize = (uint32_t)fs::file_size(sourceFile);
        uint32_t namesize = (uint32_t)archivePath.size() + 1;

        char hdr[128];
        snprintf(hdr, sizeof(hdr),
                 "070701%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x",
                 1, mode, 0, 0, 1, (uint32_t)time(nullptr), filesize, 0, 0, 0, 0, namesize, 0);

        out.write(hdr, 110);
        out.write(archivePath.c_str(), namesize);
        size_t padName = (4 - ((110 + namesize) % 4)) % 4;
        for (size_t i = 0; i < padName; ++i) out.put('\0');

        std::ifstream in(sourceFile, std::ios::binary);
        char buf[65536];
        while (in.read(buf, sizeof(buf)) || in.gcount() > 0) {
            out.write(buf, in.gcount());
        }

        size_t padData = (4 - (filesize % 4)) % 4;
        for (size_t i = 0; i < padData; ++i) out.put('\0');
    }

void CpioCompiler::appendTrailer(std::ostream& out) {
        char hdr[128];
        std::string t = "TRAILER!!!";
        uint32_t namesize = (uint32_t)t.size() + 1;
        snprintf(hdr, sizeof(hdr),
                 "070701%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x",
                 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, namesize, 0);
        out.write(hdr, 110);
        out.write(t.c_str(), namesize);
        size_t padName = (4 - ((110 + namesize) % 4)) % 4;
        for (size_t i = 0; i < padName; ++i) out.put('\0');
    }
