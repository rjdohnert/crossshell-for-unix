#ifndef PAX_HPP
#define PAX_HPP

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <regex>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <chrono>

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct TarHeader {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
};
#pragma pack(pop)

static_assert(sizeof(TarHeader) == 512, "TarHeader size must be 512 bytes");

class OctalCodec {
public:
    static uint64_t ParseOctal(const char* str, size_t size);
    static void FormatOctal(char* dest, size_t size, uint64_t val);
    static uint32_t CalculateChecksum(const TarHeader& hdr);
    static std::string GetFullTarPath(const TarHeader& hdr);
};

struct Substitution {
    std::regex re;
    std::string replacement;
    bool global = false;
};

class SubstitutionEngine {
public:
    static bool ParseSubstitution(const std::string& arg, std::vector<Substitution>& subs);
    static std::string ApplySubstitutions(std::string path, const std::vector<Substitution>& subs);
};

#endif // PAX_HPP
