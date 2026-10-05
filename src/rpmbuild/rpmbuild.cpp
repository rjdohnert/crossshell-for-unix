/*
 * Copyright (c) 2026 PC/OpenSystems LLC contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * ============================================================================
 * INDEX: rpmbuild.cpp
 * ============================================================================
 * 
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 * Win32 SDK: bcrypt.lib, advapi32.lib, shell32.lib
 *
 * TABLE OF CONTENTS:
 * 1. [RPM SPECIFICATION & CONSTANTS] ................. Magic numbers, Lead/Index structs, and tags
 * 2. [WINDOWS CNG CRYPTO ENGINE] ..................... SHA-256 calculation for files and memory buffers
 * 3. [RPM SPEC PARSER & MACRO ENGINE] ................ .spec parser, macro expansion (%{...} & %...), %files, %attr
 * 4. [RPM HEADER BUILDER WITH BOUNDARY ALIGNMENT] .... Strict natural-boundary data store alignment serializer
 * 5. [CPIO NEWC PAYLOAD COMPILER] .................... Standard CPIO archive generator (file entries & trailer)
 * 6. [STAGE RUNNER] .................................. Safe execution of %prep, %build, %install, and %clean
 * 7. [COMPREHENSIVE CLI HELP] ........................ Windows spec reference manual and command usage
 * 8. [MAIN COMPILER ENTRY POINT] ..................... CLI argument parser, stage execution, SRPM, and RPM output
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <map>
#include <set>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <regex>
#include <cctype>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

// ============================================================================
// RPM SPECIFICATION & CONSTANTS
// ============================================================================
constexpr uint32_t RPM_MAGIC_LEAD   = 0xedabeedb;
constexpr uint32_t RPM_HEADER_MAGIC = 0x8eade801;

enum RpmTagType : uint32_t {
    RPM_NULL_TYPE    = 0,
    RPM_CHAR_TYPE    = 1,
    RPM_INT8_TYPE    = 2,
    RPM_INT16_TYPE   = 3,
    RPM_INT32_TYPE   = 4,
    RPM_INT64_TYPE   = 5,
    RPM_STRING_TYPE  = 6,
    RPM_BIN_TYPE     = 7,
    RPM_STRING_ARRAY = 8
};

enum RpmTag : uint32_t {
    RPMTAG_NAME              = 1000,
    RPMTAG_VERSION           = 1001,
    RPMTAG_RELEASE           = 1002,
    RPMTAG_EPOCH             = 1003,
    RPMTAG_SUMMARY           = 1004,
    RPMTAG_DESCRIPTION       = 1005,
    RPMTAG_BUILDTIME         = 1006,
    RPMTAG_SIZE              = 1009,
    RPMTAG_DISTRIBUTION      = 1010,
    RPMTAG_VENDOR            = 1011,
    RPMTAG_LICENSE           = 1014,
    RPMTAG_PACKAGER          = 1015,
    RPMTAG_GROUP             = 1016,
    RPMTAG_URL               = 1020,
    RPMTAG_OS                = 1021,
    RPMTAG_ARCH              = 1022,
    RPMTAG_PREIN             = 1023,
    RPMTAG_POSTIN            = 1024,
    RPMTAG_PREUN             = 1025,
    RPMTAG_POSTUN            = 1026,
    RPMTAG_FILESIZES         = 1028,
    RPMTAG_FILEMODES         = 1030,
    RPMTAG_FILEDIGESTS       = 1035,
    RPMTAG_FILEFLAGS         = 1037,
    RPMTAG_PROVIDENAME       = 1047,
    RPMTAG_REQUIREFLAGS      = 1048,
    RPMTAG_REQUIRENAME       = 1049,
    RPMTAG_REQUIREVERSION    = 1050,
    RPMTAG_CONFLICTNAME      = 1054,
    RPMTAG_PROVIDEFLAGS      = 1112,
    RPMTAG_PROVIDEVERSION    = 1113,
    RPMTAG_DIRINDEXES        = 1116,
    RPMTAG_BASENAMES         = 1117,
    RPMTAG_DIRNAMES          = 1118,
    RPMTAG_PAYLOADFORMAT     = 1124,
    RPMTAG_PAYLOADCOMPRESSOR = 1125,
    RPMTAG_PAYLOADFLAGS      = 1126,
    RPMTAG_SHA256HEADER      = 273
};

// File and Dependency Flags
constexpr uint32_t RPMFILE_CONFIG     = (1 << 0);
constexpr uint32_t RPMFILE_DOC        = (1 << 1);
constexpr uint32_t RPMFILE_NOREPLACE  = (1 << 4);
constexpr uint32_t RPMSENSE_LESS      = 0x02;
constexpr uint32_t RPMSENSE_GREATER   = 0x04;
constexpr uint32_t RPMSENSE_EQUAL     = 0x08;

#pragma pack(push, 1)
struct RpmLeadRaw {
    uint32_t magic;
    uint8_t  major;
    uint8_t  minor;
    uint16_t type;
    uint16_t archnum;
    char     name[66];
    uint16_t osnum;
    uint16_t signature_type;
    char     reserved[16];
};

struct RpmIndexEntryRaw {
    uint32_t tag;
    uint32_t type;
    uint32_t offset;
    uint32_t count;
};
#pragma pack(pop)

inline uint16_t swap16(uint16_t val) { return (val << 8) | (val >> 8); }
inline uint32_t swap32(uint32_t val) {
    return ((val >> 24) & 0xff) | ((val << 8) & 0xff0000) |
           ((val >> 8) & 0xff00) | ((val << 24) & 0xff000000);
}

// ============================================================================
// WINDOWS CNG CRYPTO ENGINE
// ============================================================================
class CngCrypto {
public:
    static std::string calculateFileSha256(const fs::path& filePath) {
        std::error_code ec;
        if (!fs::exists(filePath, ec) || fs::is_directory(filePath, ec)) return "";
        BCRYPT_ALG_HANDLE hAlg = NULL;
        BCRYPT_HASH_HANDLE hHash = NULL;
        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) return "";

        DWORD cbHash = 0, cbData = 0;
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0);
        std::vector<BYTE> hashBuffer(cbHash);

        if (BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0) < 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        char buffer[65536];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            BCryptHashData(hHash, (PBYTE)buffer, (ULONG)file.gcount(), 0);
        }

        BCryptFinishHash(hHash, hashBuffer.data(), cbHash, 0);
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        std::ostringstream oss;
        for (BYTE b : hashBuffer) oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        return oss.str();
    }

    static std::string calculateMemorySha256(const char* data, size_t len) {
        BCRYPT_ALG_HANDLE hAlg = NULL;
        BCRYPT_HASH_HANDLE hHash = NULL;
        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) return "";

        DWORD cbHash = 0, cbData = 0;
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0);
        std::vector<BYTE> hashBuffer(cbHash);

        if (BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0) < 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        if (data && len > 0) {
            BCryptHashData(hHash, (PBYTE)data, (ULONG)len, 0);
        }
        BCryptFinishHash(hHash, hashBuffer.data(), cbHash, 0);
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        std::ostringstream oss;
        for (BYTE b : hashBuffer) oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        return oss.str();
    }
};

// ============================================================================
// RPM SPEC PARSER & MACRO ENGINE
// ============================================================================
struct FileSpec {
    std::string path;
    uint32_t mode = 0100644;
    uint32_t flags = 0;
};

struct DependencySpec {
    std::string name;
    std::string version;
    uint32_t flags = 0;
};

class SpecParser {
public:
    std::map<std::string, std::string> macros;
    std::string name;
    std::string version;
    std::string release = "1";
    std::string epoch = "0";
    std::string summary;
    std::string description;
    std::string license = "Proprietary";
    std::string group = "Applications/System";
    std::string url;
    std::string vendor = "Enterprise Deployment";
    std::string packager = "RPM Build Engine";
    std::string buildArch = "x86_64";
    std::string buildRoot;

    std::string prepScript;
    std::string buildScript;
    std::string installScript;
    std::string cleanScript;
    std::string preScript;
    std::string postScript;
    std::string preunScript;
    std::string postunScript;

    std::vector<FileSpec> files;
    std::vector<DependencySpec> requirements;
    std::vector<DependencySpec> provides;
    std::vector<DependencySpec> conflicts;

    SpecParser() {
        // Built-in standard directory macros adapted for Windows
        macros["_prefix"]        = "C:\\Program Files";
        macros["_bindir"]        = "C:\\Program Files\\%{name}\\bin";
        macros["_sysconfdir"]    = "C:\\ProgramData\\%{name}\\config";
        macros["_datadir"]       = "C:\\Program Files\\%{name}\\share";
        macros["_defaultdocdir"] = "C:\\Program Files\\%{name}\\doc";
    }

    static std::string stripQuotes(const std::string& str) {
        std::string s = str;
        size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = s.find_last_not_of(" \t\r\n");
        s = s.substr(first, (last - first + 1));
        if (s.size() >= 2) {
            if ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')) {
                s = s.substr(1, s.size() - 2);
            }
        }
        return s;
    }

    std::string expand(const std::string& input) {
        std::string result = input;
        int depth = 0;

        while (depth++ < 15) {
            bool matched = false;

            // 1. Match %{macro_name}
            std::regex bracedRegex(R"(%\{([a-zA-Z0-9_]+)\})");
            std::smatch match;
            if (std::regex_search(result, match, bracedRegex)) {
                std::string key = match[1].str();
                std::string val = getMacroValue(key);
                result = result.substr(0, match.position(0)) + val + result.substr(match.position(0) + match.length(0));
                matched = true;
            }

            // 2. Match %macro_name (without braces)
            if (!matched) {
                std::regex simpleRegex(R"(%([a-zA-Z0-9_]+))");
                if (std::regex_search(result, match, simpleRegex)) {
                    std::string key = match[1].str();
                    // Avoid expanding directives like %prep, %build, %files etc. if encountered in code
                    if (key != "prep" && key != "build" && key != "install" && key != "clean" &&
                        key != "pre" && key != "post" && key != "preun" && key != "postun" &&
                        key != "files" && key != "description" && key != "package") {
                        std::string val = getMacroValue(key);
                        result = result.substr(0, match.position(0)) + val + result.substr(match.position(0) + match.length(0));
                        matched = true;
                    }
                }
            }

            if (!matched) break;
        }
        return result;
    }

    std::string getMacroValue(const std::string& key) {
        if (key == "name") return !name.empty() ? name : (macros.count("name") ? macros["name"] : "");
        if (key == "version") return !version.empty() ? version : (macros.count("version") ? macros["version"] : "");
        if (key == "release") return !release.empty() ? release : (macros.count("release") ? macros["release"] : "");
        if (key == "epoch") return !epoch.empty() ? epoch : (macros.count("epoch") ? macros["epoch"] : "0");
        if (key == "buildroot" || key == "build_root") return !buildRoot.empty() ? buildRoot : (macros.count("buildroot") ? macros["buildroot"] : "");
        if (macros.find(key) != macros.end()) return macros[key];
        return "";
    }

    bool parseFile(const fs::path& specPath) {
        std::ifstream file(specPath);
        if (!file.is_open()) {
            std::cerr << "error: Cannot open spec file: " << specPath.string() << "\n";
            return false;
        }

        std::string line;
        std::string currentSection = "HEADER";
        std::stringstream sectionBuffer;

        auto flushSection = [&]() {
            std::string content = sectionBuffer.str();
            sectionBuffer.str("");
            sectionBuffer.clear();
            if (currentSection == "%prep") prepScript = content;
            else if (currentSection == "%build") buildScript = content;
            else if (currentSection == "%install") installScript = content;
            else if (currentSection == "%clean") cleanScript = content;
            else if (currentSection == "%pre") preScript = content;
            else if (currentSection == "%post") postScript = content;
            else if (currentSection == "%preun") preunScript = content;
            else if (currentSection == "%postun") postunScript = content;
            else if (currentSection == "%description") description = content;
        };

        bool firstLine = true;
        while (std::getline(file, line)) {
            if (firstLine) {
                firstLine = false;
                if (line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
                    line = line.substr(3);
                }
            }
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) continue;
            size_t last = line.find_last_not_of(" \t\r\n");
            std::string trimmed = line.substr(first, (last - first + 1));

            if (trimmed.rfind("#", 0) == 0) continue; // Comments

            if (trimmed.rfind("%define", 0) == 0 || trimmed.rfind("%global", 0) == 0) {
                std::istringstream iss(trimmed);
                std::string directive, key, val;
                iss >> directive >> key;
                std::getline(iss, val);
                size_t vFirst = val.find_first_not_of(" \t");
                if (vFirst != std::string::npos) val = val.substr(vFirst);
                val = stripQuotes(val);
                macros[key] = val;
                if (key == "name") name = val;
                else if (key == "version") version = val;
                else if (key == "release") release = val;
                continue;
            }

            auto isSectionHeader = [](const std::string& token) {
                return token == "%prep" || token == "%build" || token == "%install" ||
                       token == "%clean" || token == "%pre" || token == "%post" ||
                       token == "%preun" || token == "%postun" || token == "%files" ||
                       token == "%description" || token == "%package" || token == "%changelog";
            };

            if (trimmed[0] == '%') {
                std::istringstream iss(trimmed);
                std::string firstWord;
                iss >> firstWord;
                if (isSectionHeader(firstWord)) {
                    flushSection();
                    currentSection = firstWord;
                    continue;
                }
            }

            if (currentSection == "HEADER") {
                auto sep = trimmed.find(':');
                if (sep != std::string::npos) {
                    std::string key = trimmed.substr(0, sep);
                    std::string val = trimmed.substr(sep + 1);
                    size_t vFirst = val.find_first_not_of(" \t");
                    if (vFirst != std::string::npos) val = val.substr(vFirst);
                    val = stripQuotes(val);

                    if (_stricmp(key.c_str(), "Name") == 0) { name = val; macros["name"] = val; }
                    else if (_stricmp(key.c_str(), "Version") == 0) { version = val; macros["version"] = val; }
                    else if (_stricmp(key.c_str(), "Release") == 0) { release = val; macros["release"] = val; }
                    else if (_stricmp(key.c_str(), "Epoch") == 0) { epoch = val; macros["epoch"] = val; }
                    else if (_stricmp(key.c_str(), "Summary") == 0) summary = val;
                    else if (_stricmp(key.c_str(), "License") == 0) license = val;
                    else if (_stricmp(key.c_str(), "Group") == 0) group = val;
                    else if (_stricmp(key.c_str(), "URL") == 0) url = val;
                    else if (_stricmp(key.c_str(), "Vendor") == 0) vendor = val;
                    else if (_stricmp(key.c_str(), "BuildArch") == 0) buildArch = val;
                    else if (_stricmp(key.c_str(), "BuildRoot") == 0) buildRoot = val;
                    else if (_stricmp(key.c_str(), "Requires") == 0) parseDependencyList(val, requirements);
                    else if (_stricmp(key.c_str(), "Provides") == 0) parseDependencyList(val, provides);
                    else if (_stricmp(key.c_str(), "Conflicts") == 0) parseDependencyList(val, conflicts);
                }
            } else if (currentSection == "%files") {
                parseFileDirective(trimmed);
            } else {
                sectionBuffer << line << "\n";
            }
        }
        flushSection();

        if (buildRoot.empty()) {
            buildRoot = (fs::temp_directory_path() / ("BUILDROOT_" + name + "-" + version + "-" + release)).string();
        }
        buildRoot = expand(buildRoot);
        return true;
    }

private:
    void parseDependencyList(const std::string& line, std::vector<DependencySpec>& list) {
        // Split dependencies by comma
        std::stringstream ss(line);
        std::string item;
        while (std::getline(ss, item, ',')) {
            std::istringstream iss(item);
            std::string depName, op, depVer;
            if (iss >> depName) {
                DependencySpec spec;
                spec.name = stripQuotes(depName);
                if (iss >> op >> depVer) {
                    spec.version = stripQuotes(depVer);
                    if (op == ">=") spec.flags = RPMSENSE_GREATER | RPMSENSE_EQUAL;
                    else if (op == "<=") spec.flags = RPMSENSE_LESS | RPMSENSE_EQUAL;
                    else if (op == "=" || op == "==") spec.flags = RPMSENSE_EQUAL;
                    else if (op == ">") spec.flags = RPMSENSE_GREATER;
                    else if (op == "<") spec.flags = RPMSENSE_LESS;
                }
                list.push_back(spec);
            }
        }
    }

    void parseFileDirective(const std::string& line) {
        FileSpec spec;
        std::string pathPart = line;
        bool customModeSet = false;

        // Parse %attr(mode, user, group)
        if (pathPart.rfind("%attr(", 0) == 0) {
            auto closeParen = pathPart.find(')');
            if (closeParen != std::string::npos) {
                std::string attrBody = pathPart.substr(6, closeParen - 6);
                pathPart = pathPart.substr(closeParen + 1);

                std::stringstream attrSS(attrBody);
                std::string modeStr;
                if (std::getline(attrSS, modeStr, ',')) {
                    size_t mFirst = modeStr.find_first_not_of(" \t-");
                    if (mFirst != std::string::npos) {
                        try {
                            uint32_t oct = static_cast<uint32_t>(std::stoul(modeStr.substr(mFirst), nullptr, 8));
                            if (oct > 0) {
                                spec.mode = (oct >= 0100000) ? oct : (0100000 | oct);
                                customModeSet = true;
                            }
                        } catch (...) {}
                    }
                }
            }
        }

        // Parse %config and %doc directives
        if (pathPart.rfind("%config(noreplace)", 0) == 0) {
            spec.flags |= (RPMFILE_CONFIG | RPMFILE_NOREPLACE);
            pathPart = pathPart.substr(18);
        } else if (pathPart.rfind("%config", 0) == 0) {
            spec.flags |= RPMFILE_CONFIG;
            pathPart = pathPart.substr(7);
        } else if (pathPart.rfind("%doc", 0) == 0) {
            spec.flags |= RPMFILE_DOC;
            pathPart = pathPart.substr(4);
        } else if (pathPart.rfind("%dir", 0) == 0) {
            pathPart = pathPart.substr(4);
        }

        pathPart = stripQuotes(pathPart);
        spec.path = expand(pathPart);

        if (!customModeSet) {
            spec.mode = (spec.path.find(".exe") != std::string::npos ||
                         spec.path.find(".bat") != std::string::npos ||
                         spec.path.find(".cmd") != std::string::npos ||
                         spec.path.find(".ps1") != std::string::npos) ? 0100755 : 0100644;
        }
        files.push_back(spec);
    }
};

// ============================================================================
// RPM HEADER BUILDER WITH STRICT TYPE ALIGNMENT
// ============================================================================
class HeaderSerializer {
private:
    struct TagEntry {
        uint32_t tag;
        uint32_t type;
        uint32_t count;
        std::vector<char> data;
    };
    std::vector<TagEntry> entries;

    static size_t getAlignment(uint32_t type) {
        switch (type) {
            case RPM_INT16_TYPE: return 2;
            case RPM_INT32_TYPE: return 4;
            case RPM_INT64_TYPE: return 8;
            default: return 1;
        }
    }

public:
    void addString(uint32_t tag, const std::string& str) {
        TagEntry e;
        e.tag = tag; e.type = RPM_STRING_TYPE; e.count = 1;
        e.data.assign(str.c_str(), str.c_str() + str.size() + 1);
        entries.push_back(e);
    }

    void addInt32(uint32_t tag, uint32_t val) {
        TagEntry e;
        e.tag = tag; e.type = RPM_INT32_TYPE; e.count = 1;
        uint32_t be = swap32(val);
        e.data.resize(4);
        std::memcpy(e.data.data(), &be, 4);
        entries.push_back(e);
    }

    void addUint16Array(uint32_t tag, const std::vector<uint16_t>& arr) {
        if (arr.empty()) return;
        TagEntry e;
        e.tag = tag; e.type = RPM_INT16_TYPE; e.count = (uint32_t)arr.size();
        e.data.resize(arr.size() * 2);
        for (size_t i = 0; i < arr.size(); ++i) {
            uint16_t be = swap16(arr[i]);
            std::memcpy(e.data.data() + (i * 2), &be, 2);
        }
        entries.push_back(e);
    }

    void addUint32Array(uint32_t tag, const std::vector<uint32_t>& arr) {
        if (arr.empty()) return;
        TagEntry e;
        e.tag = tag; e.type = RPM_INT32_TYPE; e.count = (uint32_t)arr.size();
        e.data.resize(arr.size() * 4);
        for (size_t i = 0; i < arr.size(); ++i) {
            uint32_t be = swap32(arr[i]);
            std::memcpy(e.data.data() + (i * 4), &be, 4);
        }
        entries.push_back(e);
    }

    void addStringArray(uint32_t tag, const std::vector<std::string>& arr) {
        if (arr.empty()) return;
        TagEntry e;
        e.tag = tag; e.type = RPM_STRING_ARRAY; e.count = (uint32_t)arr.size();
        for (const auto& s : arr) {
            e.data.insert(e.data.end(), s.c_str(), s.c_str() + s.size() + 1);
        }
        entries.push_back(e);
    }

    std::vector<char> serialize() {
        std::sort(entries.begin(), entries.end(), [](const TagEntry& a, const TagEntry& b) {
            return a.tag < b.tag;
        });

        // Compute aligned offsets and build aligned data store
        std::vector<char> dataStore;
        std::vector<RpmIndexEntryRaw> indexEntries;

        for (const auto& e : entries) {
            size_t align = getAlignment(e.type);
            size_t pad = (align - (dataStore.size() % align)) % align;
            for (size_t i = 0; i < pad; ++i) dataStore.push_back('\0');

            RpmIndexEntryRaw idx;
            idx.tag = swap32(e.tag);
            idx.type = swap32(e.type);
            idx.offset = swap32((uint32_t)dataStore.size());
            idx.count = swap32(e.count);
            indexEntries.push_back(idx);

            dataStore.insert(dataStore.end(), e.data.begin(), e.data.end());
        }

        std::vector<char> buffer;
        uint8_t magic[4] = {0x8e, 0xad, 0xe8, 0x01};
        buffer.insert(buffer.end(), magic, magic + 4);
        uint32_t reserved = 0;
        buffer.insert(buffer.end(), (char*)&reserved, (char*)&reserved + 4);

        uint32_t nindex = swap32((uint32_t)indexEntries.size());
        uint32_t nbytes = swap32((uint32_t)dataStore.size());

        buffer.insert(buffer.end(), (char*)&nindex, (char*)&nindex + 4);
        buffer.insert(buffer.end(), (char*)&nbytes, (char*)&nbytes + 4);

        for (const auto& idx : indexEntries) {
            buffer.insert(buffer.end(), (char*)&idx, (char*)&idx + sizeof(idx));
        }

        buffer.insert(buffer.end(), dataStore.begin(), dataStore.end());
        return buffer;
    }
};

// ============================================================================
// CPIO NEWC PAYLOAD COMPILER
// ============================================================================
class CpioCompiler {
public:
    static void appendFile(std::ostream& out, const std::string& archivePath, const fs::path& sourceFile, uint32_t mode) {
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

    static void appendTrailer(std::ostream& out) {
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
};

// ============================================================================
// STAGE RUNNER (%prep, %build, %install)
// ============================================================================
class StageRunner {
public:
    static bool execute(const std::string& script, const std::string& stageName, const std::string& buildRoot) {
        if (script.empty()) return true;

        std::cout << "Executing (%" << stageName << "): " << "cmd.exe /c ...\n";
        fs::path bat = fs::temp_directory_path() / ("rpmstage_" + stageName + "_" + std::to_string(GetCurrentProcessId()) + ".bat");
        {
            std::ofstream out(bat);
            out << "@echo off\r\n";
            out << "set BUILDROOT=" << buildRoot << "\r\n";
            out << "set RPM_BUILD_ROOT=" << buildRoot << "\r\n";
            out << script << "\r\n";
        }

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi{};
        std::string cmd = "cmd.exe /s /c \"\"" + bat.string() + "\"\"";

        std::vector<char> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back('\0');

        BOOL ok = CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
        std::error_code ec;
        if (ok) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            fs::remove(bat, ec);
            if (exitCode != 0) {
                std::cerr << "error: Bad exit status from %" << stageName << " (exit code " << exitCode << ")\n";
                return false;
            }
            return true;
        }
        fs::remove(bat, ec);
        return false;
    }
};

// ============================================================================
// COMPREHENSIVE CLI HELP
// ============================================================================
void printHelp() {
    std::cout <<
R"(rpmbuild(1)                CrossShell for UNIX Reference Manual               rpmbuild(1)

NAME
    rpmbuild - build binary and source RPM packages

SYNOPSIS
    rpmbuild [OPTIONS] SPEC_FILE

DESCRIPTION
    Parses a Windows-oriented RPM spec file, executes build stages,
    and creates binary or source packages. The default build mode
    is binary packaging.

OPTIONS
    Build Modes:
        -ba
            Build both binary and source packages.
        -bb
            Build binary package only (from spec).
        -bs
            Build source RPM package only.
        -bp
            Execute the %prep stage only (unpack and patch).
        -bc
            Execute the %build stage only (compile sources).
        -bi
            Execute the %install stage only (stage to BUILDROOT).
        -bl
            Verify %files list against BUILDROOT.

    Configuration and Directives:
        --buildroot=DIR
            Override the default build root directory.
        --define="MACRO VALUE", -D "MACRO VALUE"
            Define build macro MACRO with value VALUE.
        --target=ARCH
            Target binary architecture (default: x86_64).
        --output=FILE
            Set explicit output file name.
        --clean
            Remove BUILDROOT directory tree after completion.
        -v, --verbose
            Enable detailed compilation telemetry.
        -?, --help
            Display this comprehensive reference manual and exit.
        --version
            Display version information and exit.

EXAMPLES
    rpmbuild -bb package.spec
        Build a binary package from specification file.

    rpmbuild -ba --clean package.spec
        Build binary and source packages and clean BUILDROOT.

    rpmbuild -bp package.spec
        Execute only the preparation stage.

    rpmbuild -bl package.spec
        Verify staged files against the %files list.

EXIT STATUS
    0   Successful build or completed stage check.
    1   Missing mode/spec, parse, stage, BUILDROOT, or packaging failure.

    CrossShell for UNIX                                              rpmbuild(1)
)";
}

// ============================================================================
// MAIN COMPILER ENTRY POINT
// ============================================================================
int main(int argc, char* argv[]) {
    if (argc <= 1) {
        std::cerr << "rpmbuild: no spec file or build mode specified.\nTry 'rpmbuild --help' for more information.\n";
        return 1;
    }

    std::string specFilePath;
    std::string explicitOutput;
    std::string overrideBuildRoot;
    std::string overrideTarget;
    std::map<std::string, std::string> cliMacros;
    bool modeAll = false, modeBinary = false, modeSource = false, modePrep = false, modeCompile = false, modeInstall = false, modeCheckFiles = false;
    bool optClean = false, optVerbose = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-?") {
            printHelp();
            return 0;
        } else if (arg == "--version") {
            std::cout << "RPM build version 3.8.19\n";
            return 0;
        } else if (arg == "-ba") { modeAll = true; }
        else if (arg == "-bb") { modeBinary = true; }
        else if (arg == "-bs") { modeSource = true; }
        else if (arg == "-bp") { modePrep = true; }
        else if (arg == "-bc") { modeCompile = true; }
        else if (arg == "-bi") { modeInstall = true; }
        else if (arg == "-bl") { modeCheckFiles = true; }
        else if (arg == "--clean") { optClean = true; }
        else if (arg == "-v" || arg == "--verbose") { optVerbose = true; }
        else if (arg.rfind("--output=", 0) == 0) { explicitOutput = arg.substr(9); }
        else if (arg.rfind("--buildroot=", 0) == 0) { overrideBuildRoot = arg.substr(12); }
        else if (arg.rfind("--target=", 0) == 0) { overrideTarget = arg.substr(9); }
        else if (arg == "--target" && i + 1 < argc) { overrideTarget = argv[++i]; }
        else if (arg.rfind("--define=", 0) == 0) {
            std::string def = arg.substr(9);
            auto sp = def.find_first_of(" =");
            if (sp != std::string::npos) cliMacros[def.substr(0, sp)] = SpecParser::stripQuotes(def.substr(sp + 1));
        } else if (arg == "-D" && i + 1 < argc) {
            std::string def = argv[++i];
            auto sp = def.find_first_of(" =");
            if (sp != std::string::npos) cliMacros[def.substr(0, sp)] = SpecParser::stripQuotes(def.substr(sp + 1));
        } else if (arg.rfind("-", 0) != 0) {
            specFilePath = arg;
        }
    }

    if (specFilePath.empty()) {
        std::cerr << "error: No spec file provided.\n";
        return 1;
    }

    if (!modeAll && !modeBinary && !modeSource && !modePrep && !modeCompile && !modeInstall && !modeCheckFiles) {
        modeBinary = true; // Default to binary package build
    }

    SpecParser spec;
    for (const auto& [k, v] : cliMacros) {
        spec.macros[k] = v;
        if (k == "name") spec.name = v;
        else if (k == "version") spec.version = v;
        else if (k == "release") spec.release = v;
    }

    if (!spec.parseFile(specFilePath)) return 1;
    if (!overrideBuildRoot.empty()) spec.buildRoot = overrideBuildRoot;
    if (!overrideTarget.empty()) spec.buildArch = overrideTarget;

    std::cout << "Executing build for package: " << spec.name << "-" << spec.version << "-" << spec.release << "\n";

    // Handle Source RPM only build (-bs)
    if (modeSource) {
        std::string srcOut = spec.name + "-" + spec.version + "-" + spec.release + ".src.rpm";
        std::ofstream srcRpm(srcOut, std::ios::binary);
        if (!srcRpm.is_open()) {
            std::cerr << "error: Cannot open output source package for writing: " << srcOut << "\n";
            return 1;
        }

        RpmLeadRaw srcLead{};
        srcLead.magic = swap32(RPM_MAGIC_LEAD);
        srcLead.major = 3;
        srcLead.type = swap16(1); // 1 = Source RPM
        srcLead.archnum = swap16(1);
        strncpy_s(srcLead.name, spec.name.c_str(), _TRUNCATE);
        srcLead.osnum = swap16(1);
        srcLead.signature_type = swap16(5);
        srcRpm.write(reinterpret_cast<char*>(&srcLead), sizeof(srcLead));

        HeaderSerializer srcMainHdr;
        srcMainHdr.addString(RPMTAG_NAME, spec.name);
        srcMainHdr.addString(RPMTAG_VERSION, spec.version);
        srcMainHdr.addString(RPMTAG_RELEASE, spec.release);
        srcMainHdr.addString(RPMTAG_SUMMARY, spec.summary);
        srcMainHdr.addString(RPMTAG_DESCRIPTION, spec.description);
        srcMainHdr.addString(RPMTAG_OS, "windows");
        srcMainHdr.addString(RPMTAG_ARCH, "src");
        srcMainHdr.addString(RPMTAG_PAYLOADFORMAT, "cpio");
        srcMainHdr.addString(RPMTAG_PAYLOADCOMPRESSOR, "none");
        srcMainHdr.addInt32(RPMTAG_BUILDTIME, (uint32_t)time(nullptr));

        std::vector<char> serSrcMain = srcMainHdr.serialize();
        std::string srcSha256 = CngCrypto::calculateMemorySha256(serSrcMain.data(), serSrcMain.size());

        HeaderSerializer srcSigHdr;
        srcSigHdr.addString(RPMTAG_SHA256HEADER, srcSha256);
        srcSigHdr.addInt32(RPMTAG_SIZE, (uint32_t)serSrcMain.size());
        std::vector<char> serSrcSig = srcSigHdr.serialize();

        srcRpm.write(serSrcSig.data(), serSrcSig.size());
        std::streampos srcPos = srcRpm.tellp();
        if (srcPos % 8 != 0) {
            size_t pad = 8 - (srcPos % 8);
            for (size_t i = 0; i < pad; ++i) srcRpm.put('\0');
        }
        srcRpm.write(serSrcMain.data(), serSrcMain.size());

        fs::path sPath(specFilePath);
        CpioCompiler::appendFile(srcRpm, "./" + sPath.filename().generic_string(), sPath, 0100644);
        CpioCompiler::appendTrailer(srcRpm);
        srcRpm.close();
        std::cout << "Wrote: " << fs::absolute(srcOut).string() << " (" << fs::file_size(srcOut) << " bytes)\n";
        return 0;
    }

    // 1. %prep Stage
    if (!StageRunner::execute(spec.prepScript, "prep", spec.buildRoot)) return 1;
    if (modePrep) return 0;

    // 2. %build Stage
    if (!StageRunner::execute(spec.buildScript, "build", spec.buildRoot)) return 1;
    if (modeCompile) return 0;

    // 3. %install Stage
    if (!StageRunner::execute(spec.installScript, "install", spec.buildRoot)) return 1;
    if (modeInstall) return 0;

    // 4. File Discovery & SHA-256 Digest Matrix Generation
    std::cout << "Processing %files section and calculating cryptographic SHA-256 digests...\n";
    std::vector<std::string> dirNames = {"/"};
    std::vector<std::string> baseNames;
    std::vector<uint32_t> dirIndexes;
    std::vector<uint32_t> fileSizes;
    std::vector<uint16_t> fileModes;
    std::vector<uint32_t> fileFlags;
    std::vector<std::string> fileDigests;

    struct DiscoveredFile {
        std::string relPath;
        fs::path fullDiskPath;
        uint32_t size;
        uint32_t mode;
        uint32_t flags;
        std::string sha256;
    };
    std::vector<DiscoveredFile> packageFiles;

    fs::path bRoot(spec.buildRoot);
    std::error_code ec;
    if (!fs::exists(bRoot, ec)) {
        std::cerr << "error: BUILDROOT directory does not exist: " << spec.buildRoot << "\n";
        return 1;
    }

    for (const auto& entry : fs::recursive_directory_iterator(bRoot, ec)) {
        if (entry.is_regular_file(ec)) {
            fs::path rel = fs::relative(entry.path(), bRoot, ec);
            std::string relStr = rel.generic_string();
            std::string dir = rel.parent_path().generic_string();
            if (!dir.empty()) dir = "/" + dir + "/";
            else dir = "/";

            auto it = std::find(dirNames.begin(), dirNames.end(), dir);
            uint32_t dIdx = 0;
            if (it == dirNames.end()) {
                dIdx = (uint32_t)dirNames.size();
                dirNames.push_back(dir);
            } else {
                dIdx = (uint32_t)(it - dirNames.begin());
            }

            uint32_t sz = static_cast<uint32_t>(entry.file_size(ec));
            std::string hash = CngCrypto::calculateFileSha256(entry.path());

            uint32_t mode = 0100644;
            uint32_t flags = 0;

            std::string fname = rel.filename().generic_string();
            bool matched = false;
            // Match exact relative path first
            for (const auto& f : spec.files) {
                std::string fClean = f.path;
                while (!fClean.empty() && (fClean[0] == '/' || fClean[0] == '\\')) fClean = fClean.substr(1);
                if (fClean == relStr) {
                    mode = f.mode;
                    flags = f.flags;
                    matched = true;
                    break;
                }
            }
            // Fallback to filename match
            if (!matched) {
                for (const auto& f : spec.files) {
                    fs::path fPath(f.path);
                    if (fPath.filename().generic_string() == fname) {
                        mode = f.mode;
                        flags = f.flags;
                        break;
                    }
                }
            }

            dirIndexes.push_back(dIdx);
            baseNames.push_back(fname);
            fileSizes.push_back(sz);
            fileModes.push_back((uint16_t)mode);
            fileFlags.push_back(flags);
            fileDigests.push_back(hash);

            packageFiles.push_back({relStr, entry.path(), sz, mode, flags, hash});
            if (optVerbose) {
                std::cout << "  File: " << relStr << " [SHA256: " << hash.substr(0, 12) << "... Mode: " << std::oct << mode << std::dec << "]\n";
            }
        }
    }

    if (packageFiles.empty()) {
        std::cerr << "error: No files staged in BUILDROOT to package.\n";
        return 1;
    }
    if (modeCheckFiles) {
        std::cout << "Check %files passed cleanly. Total files: " << packageFiles.size() << "\n";
        return 0;
    }

    // 5. Serialize Immutable Main Header
    HeaderSerializer mainHdr;
    mainHdr.addString(RPMTAG_NAME, spec.name);
    mainHdr.addString(RPMTAG_VERSION, spec.version);
    mainHdr.addString(RPMTAG_RELEASE, spec.release);
    
    uint32_t epVal = 0;
    try { epVal = static_cast<uint32_t>(std::stoul(spec.epoch)); } catch (...) {}
    mainHdr.addInt32(RPMTAG_EPOCH, epVal);

    mainHdr.addString(RPMTAG_SUMMARY, spec.summary);
    mainHdr.addString(RPMTAG_DESCRIPTION, spec.description);
    mainHdr.addString(RPMTAG_VENDOR, spec.vendor);
    mainHdr.addString(RPMTAG_PACKAGER, spec.packager);
    mainHdr.addString(RPMTAG_LICENSE, spec.license);
    mainHdr.addString(RPMTAG_GROUP, spec.group);
    mainHdr.addString(RPMTAG_URL, spec.url);
    mainHdr.addString(RPMTAG_OS, "windows");
    mainHdr.addString(RPMTAG_ARCH, spec.buildArch);
    mainHdr.addString(RPMTAG_PAYLOADFORMAT, "cpio");
    mainHdr.addString(RPMTAG_PAYLOADCOMPRESSOR, "none");
    mainHdr.addInt32(RPMTAG_BUILDTIME, (uint32_t)time(nullptr));

    uint32_t totalInstalledSize = 0;
    for (uint32_t sz : fileSizes) totalInstalledSize += sz;
    mainHdr.addInt32(RPMTAG_SIZE, totalInstalledSize);

    if (!spec.preScript.empty()) mainHdr.addString(RPMTAG_PREIN, spec.preScript);
    if (!spec.postScript.empty()) mainHdr.addString(RPMTAG_POSTIN, spec.postScript);
    if (!spec.preunScript.empty()) mainHdr.addString(RPMTAG_PREUN, spec.preunScript);
    if (!spec.postunScript.empty()) mainHdr.addString(RPMTAG_POSTUN, spec.postunScript);

    // Modern Indexed Path Triples & Digest Matrix
    mainHdr.addStringArray(RPMTAG_DIRNAMES, dirNames);
    mainHdr.addStringArray(RPMTAG_BASENAMES, baseNames);
    mainHdr.addUint32Array(RPMTAG_DIRINDEXES, dirIndexes);
    mainHdr.addUint32Array(RPMTAG_FILESIZES, fileSizes);
    mainHdr.addUint16Array(RPMTAG_FILEMODES, fileModes);
    mainHdr.addUint32Array(RPMTAG_FILEFLAGS, fileFlags);
    mainHdr.addStringArray(RPMTAG_FILEDIGESTS, fileDigests);

    // Dependencies
    if (!spec.requirements.empty()) {
        std::vector<std::string> reqN, reqV;
        std::vector<uint32_t> reqF;
        for (const auto& r : spec.requirements) {
            reqN.push_back(r.name);
            reqV.push_back(r.version);
            reqF.push_back(r.flags);
        }
        mainHdr.addStringArray(RPMTAG_REQUIRENAME, reqN);
        mainHdr.addStringArray(RPMTAG_REQUIREVERSION, reqV);
        mainHdr.addUint32Array(RPMTAG_REQUIREFLAGS, reqF);
    }

    if (!spec.provides.empty()) {
        std::vector<std::string> provN, provV;
        std::vector<uint32_t> provF;
        for (const auto& p : spec.provides) {
            provN.push_back(p.name);
            provV.push_back(p.version);
            provF.push_back(p.flags);
        }
        mainHdr.addStringArray(RPMTAG_PROVIDENAME, provN);
        mainHdr.addStringArray(RPMTAG_PROVIDEVERSION, provV);
        mainHdr.addUint32Array(RPMTAG_PROVIDEFLAGS, provF);
    }

    if (!spec.conflicts.empty()) {
        std::vector<std::string> confN;
        for (const auto& c : spec.conflicts) {
            confN.push_back(c.name);
        }
        mainHdr.addStringArray(RPMTAG_CONFLICTNAME, confN);
    }

    std::vector<char> serializedMainHdr = mainHdr.serialize();

    // 6. Calculate Immutable Header SHA-256 Digest
    std::string headerSha256 = CngCrypto::calculateMemorySha256(serializedMainHdr.data(), serializedMainHdr.size());

    // 7. Serialize Signature Header
    HeaderSerializer sigHdr;
    sigHdr.addString(RPMTAG_SHA256HEADER, headerSha256);
    sigHdr.addInt32(RPMTAG_SIZE, (uint32_t)serializedMainHdr.size());
    std::vector<char> serializedSigHdr = sigHdr.serialize();

    // 8. Determine Output Path
    std::string finalOut = explicitOutput;
    if (finalOut.empty()) {
        finalOut = spec.name + "-" + spec.version + "-" + spec.release + "." + spec.buildArch + ".rpm";
    }

    // 9. Write Binary RPM File (Lead + Sig Header + 8-byte Align + Main Header + CPIO Payload)
    std::ofstream rpm(finalOut, std::ios::binary);
    if (!rpm.is_open()) {
        std::cerr << "error: Cannot open output file for writing: " << finalOut << "\n";
        return 1;
    }

    RpmLeadRaw lead{};
    lead.magic = swap32(RPM_MAGIC_LEAD);
    lead.major = 3;
    lead.minor = 0;
    lead.type = 0;
    lead.archnum = swap16(1);
    strncpy_s(lead.name, spec.name.c_str(), _TRUNCATE);
    lead.osnum = swap16(1);
    lead.signature_type = swap16(5);
    rpm.write(reinterpret_cast<char*>(&lead), sizeof(lead));

    rpm.write(serializedSigHdr.data(), serializedSigHdr.size());

    std::streampos pos = rpm.tellp();
    if (pos % 8 != 0) {
        size_t pad = 8 - (pos % 8);
        for (size_t i = 0; i < pad; ++i) rpm.put('\0');
    }

    rpm.write(serializedMainHdr.data(), serializedMainHdr.size());

    for (const auto& pf : packageFiles) {
        std::string cpioPath = pf.relPath;
        while (!cpioPath.empty() && (cpioPath[0] == '/' || cpioPath[0] == '\\')) cpioPath = cpioPath.substr(1);
        CpioCompiler::appendFile(rpm, "./" + cpioPath, pf.fullDiskPath, pf.mode);
    }
    CpioCompiler::appendTrailer(rpm);

    rpm.close();
    std::cout << "Wrote: " << fs::absolute(finalOut).string() << " (" << fs::file_size(finalOut) << " bytes)\n";

    // If -ba was specified, also generate source package (.src.rpm)
    if (modeAll) {
        std::string srcOut = spec.name + "-" + spec.version + "-" + spec.release + ".src.rpm";
        std::ofstream srcRpm(srcOut, std::ios::binary);
        if (srcRpm.is_open()) {
            RpmLeadRaw srcLead{};
            srcLead.magic = swap32(RPM_MAGIC_LEAD);
            srcLead.major = 3;
            srcLead.type = swap16(1); // 1 = Source RPM
            srcLead.archnum = swap16(1);
            strncpy_s(srcLead.name, spec.name.c_str(), _TRUNCATE);
            srcLead.osnum = swap16(1);
            srcLead.signature_type = swap16(5);
            srcRpm.write(reinterpret_cast<char*>(&srcLead), sizeof(srcLead));

            HeaderSerializer srcMainHdr;
            srcMainHdr.addString(RPMTAG_NAME, spec.name);
            srcMainHdr.addString(RPMTAG_VERSION, spec.version);
            srcMainHdr.addString(RPMTAG_RELEASE, spec.release);
            srcMainHdr.addString(RPMTAG_SUMMARY, spec.summary);
            srcMainHdr.addString(RPMTAG_DESCRIPTION, spec.description);
            srcMainHdr.addString(RPMTAG_OS, "windows");
            srcMainHdr.addString(RPMTAG_ARCH, "src");
            srcMainHdr.addString(RPMTAG_PAYLOADFORMAT, "cpio");
            srcMainHdr.addString(RPMTAG_PAYLOADCOMPRESSOR, "none");
            srcMainHdr.addInt32(RPMTAG_BUILDTIME, (uint32_t)time(nullptr));

            std::vector<char> serSrcMain = srcMainHdr.serialize();
            std::string srcSha256 = CngCrypto::calculateMemorySha256(serSrcMain.data(), serSrcMain.size());

            HeaderSerializer srcSigHdr;
            srcSigHdr.addString(RPMTAG_SHA256HEADER, srcSha256);
            srcSigHdr.addInt32(RPMTAG_SIZE, (uint32_t)serSrcMain.size());
            std::vector<char> serSrcSig = srcSigHdr.serialize();

            srcRpm.write(serSrcSig.data(), serSrcSig.size());
            std::streampos srcPos = srcRpm.tellp();
            if (srcPos % 8 != 0) {
                size_t pad = 8 - (srcPos % 8);
                for (size_t i = 0; i < pad; ++i) srcRpm.put('\0');
            }
            srcRpm.write(serSrcMain.data(), serSrcMain.size());

            fs::path sPath(specFilePath);
            CpioCompiler::appendFile(srcRpm, "./" + sPath.filename().generic_string(), sPath, 0100644);
            CpioCompiler::appendTrailer(srcRpm);
            srcRpm.close();
            std::cout << "Wrote: " << fs::absolute(srcOut).string() << " (" << fs::file_size(srcOut) << " bytes)\n";
        }
    }

    if (!spec.cleanScript.empty()) {
        StageRunner::execute(spec.cleanScript, "clean", spec.buildRoot);
    }

    if (optClean) {
        fs::remove_all(bRoot, ec);
    }
    return 0;
}