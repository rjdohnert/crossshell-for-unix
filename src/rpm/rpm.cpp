/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * ============================================================================
 * INDEX: rpm.cpp
 * ============================================================================
 * 
 * Specification: C++17 | Platform: Windows 10/11 / Windows Server (x64 / ARM64)
 * Win32 SDK: bcrypt.lib, advapi32.lib, shell32.lib, ole32.lib
 *
 * TABLE OF CONTENTS:
 * 1.  [RPM SPECIFICATION DEFINITIONS] ...... RPM Magic, Lead, Tags, and Byte Swapping
 * 2.  [BASE64 SERIALIZATION UTILITIES] ..... Encode/Decode for Multiline Database Strings
 * 3.  [SECURITY ENGINE] .................... Administrator Privilege & NT ACL Converter
 * 4.  [CNG CRYPTO ENGINE] .................. BCrypt-Powered SHA-256 File & Digest Engine
 * 5.  [RPM EVR ENGINE] ..................... Official RPM 4.19 EVR Version Comparison
 * 6.  [RPM DATA STRUCTURES & PARSER] ....... RpmMetadata, RpmPackage, Modern Header Reader
 * 7.  [PERSISTENT WAL JOURNAL] ............. Crash Recovery, Staging, Rollback & Commit
 * 8.  [ARCHIVE EXTRACTION ENGINE] .......... CPIO & Decompression (Native tar.exe fallback)
 * 9.  [RPM DATABASE ENGINE] ................ Manifest DB (%ProgramData%\RPM), File Lock
 * 10. [SYSTEM SCRIPTLET RUNNER] ........... Native Windows Batch & CLI Script Execution
 * 11. [DEPENDENCY ENGINE] ................. Requirement & Virtual Capability Resolver
 * 12. [CLI FORMATTING & HELP] ............. Progress Bar & Command Usage Manual
 * 13. [MAIN DISPATCHER] ................... CLI Parsing, Query, Install, Upgrade, Erase, Verify
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <aclapi.h>
#include <sddl.h>
#include <shlobj.h>
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
#include <system_error>
#include <cctype>

#if defined(USE_LIBARCHIVE)
#include <archive.h>
#include <archive_entry.h>
#pragma comment(lib, "archive.lib")
#endif

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace fs = std::filesystem;

// ============================================================================
// 1. RPM SPECIFICATION DEFINITIONS
// ============================================================================
constexpr uint32_t RPM_MAGIC_LEAD   = 0xedabeedb;
constexpr uint32_t RPM_HEADER_MAGIC = 0x8eade801;

enum RpmTagType : uint32_t {
    RPM_NULL_TYPE       = 0,
    RPM_CHAR_TYPE       = 1,
    RPM_INT8_TYPE       = 2,
    RPM_INT16_TYPE      = 3,
    RPM_INT32_TYPE      = 4,
    RPM_INT64_TYPE      = 5,
    RPM_STRING_TYPE     = 6,
    RPM_BIN_TYPE        = 7,
    RPM_STRING_ARRAY    = 8,
    RPM_I18NSTRING_TYPE = 9
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
    RPMTAG_OLDFILENAMES      = 1027,
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

constexpr uint32_t RPMSENSE_LESS    = 0x02;
constexpr uint32_t RPMSENSE_GREATER = 0x04;
constexpr uint32_t RPMSENSE_EQUAL   = 0x08;

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
inline uint64_t swap64(uint64_t val) {
    return ((val & 0xFF00000000000000ULL) >> 56) |
           ((val & 0x00FF000000000000ULL) >> 40) |
           ((val & 0x0000FF0000000000ULL) >> 24) |
           ((val & 0x000000FF00000000ULL) >> 8)  |
           ((val & 0x00000000FF000000ULL) << 8)  |
           ((val & 0x0000000000FF0000ULL) << 24) |
           ((val & 0x000000000000FF00ULL) << 40) |
           ((val & 0x00000000000000FFULL) << 56);
}

// ============================================================================
// 2. BASE64 SERIALIZATION UTILITIES
// ============================================================================
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64Encode(const std::string& in) {
    std::string out;
    int val = 0, valb = -6;
    for (uint8_t c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64_table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b64_table[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

static std::string base64Decode(const std::string& in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[(unsigned char)b64_table[i]] = i;
    int val = 0, valb = -8;
    for (uint8_t c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

// ============================================================================
// 3. SECURITY & POSIX-TO-WINDOWS NT ACL CONVERTER
// ============================================================================
class SecurityEngine {
public:
    static bool isAdministrator() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                      DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            CheckTokenMembership(NULL, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }
        return isAdmin == TRUE;
    }

    static bool applyPosixPermissions(const fs::path& filePath, uint32_t posixMode) {
        DWORD permissions = GENERIC_READ;
        if (posixMode & 0002) permissions |= GENERIC_WRITE;
        if ((posixMode & 0001) || (posixMode & 0100)) permissions |= GENERIC_EXECUTE;

        PSID pAdminSid = NULL, pEveryoneSid = NULL;
        SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
        SID_IDENTIFIER_AUTHORITY worldAuth = SECURITY_WORLD_SID_AUTHORITY;

        if (!AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdminSid) ||
            !AllocateAndInitializeSid(&worldAuth, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &pEveryoneSid)) {
            if (pAdminSid) FreeSid(pAdminSid);
            return false;
        }

        EXPLICIT_ACCESS_W ea[2] = {0};
        ea[0].grfAccessPermissions = GENERIC_ALL;
        ea[0].grfAccessMode = SET_ACCESS;
        ea[0].grfInheritance = NO_INHERITANCE;
        ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[0].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
        ea[0].Trustee.ptstrName = (LPWSTR)pAdminSid;

        ea[1].grfAccessPermissions = permissions;
        ea[1].grfAccessMode = SET_ACCESS;
        ea[1].grfInheritance = NO_INHERITANCE;
        ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea[1].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea[1].Trustee.ptstrName = (LPWSTR)pEveryoneSid;

        PACL pNewDacl = NULL;
        DWORD dwRes = SetEntriesInAclW(2, ea, NULL, &pNewDacl);
        if (dwRes == ERROR_SUCCESS) {
            std::wstring wpath = filePath.wstring();
            dwRes = SetNamedSecurityInfoW(wpath.data(), SE_FILE_OBJECT,
                                          DACL_SECURITY_INFORMATION, NULL, NULL, pNewDacl, NULL);
            LocalFree(pNewDacl);
        }

        FreeSid(pAdminSid);
        FreeSid(pEveryoneSid);
        return dwRes == ERROR_SUCCESS;
    }
};

// ============================================================================
// 4. WINDOWS CNG CRYPTO ENGINE
// ============================================================================
class CngCryptoEngine {
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
};

// ============================================================================
// 5. RPM EVR (Epoch-Version-Release) ENGINE
// ============================================================================
class EvrEngine {
public:
    static int rpmvercmp(const std::string& a, const std::string& b) {
        if (a == b) return 0;
        size_t ix1 = 0, ix2 = 0;
        while (ix1 < a.size() || ix2 < b.size()) {
            while (ix1 < a.size() && !isalnum((unsigned char)a[ix1]) && a[ix1] != '~' && a[ix1] != '^') ix1++;
            while (ix2 < b.size() && !isalnum((unsigned char)b[ix2]) && b[ix2] != '~' && b[ix2] != '^') ix2++;

            // Handle tilde separator (sorts earlier than anything)
            if (ix1 < a.size() && a[ix1] == '~') {
                if (ix2 >= b.size() || b[ix2] != '~') return -1;
                ix1++; ix2++; continue;
            }
            if (ix2 < b.size() && b[ix2] == '~') return 1;

            // Handle caret separator (sorts earlier than normal, later than tilde)
            if (ix1 < a.size() && a[ix1] == '^') {
                if (ix2 >= b.size()) return 1;
                if (b[ix2] != '^') return -1;
                ix1++; ix2++; continue;
            }
            if (ix2 < b.size() && b[ix2] == '^') return (ix1 >= a.size()) ? -1 : 1;

            if (ix1 >= a.size() || ix2 >= b.size()) break;

            bool isNum1 = isdigit((unsigned char)a[ix1]);
            bool isNum2 = isdigit((unsigned char)b[ix2]);

            // Digits are always newer than letters
            if (isNum1 && !isNum2) return 1;
            if (!isNum1 && isNum2) return -1;

            size_t start1 = ix1, start2 = ix2;

            if (isNum1 && isNum2) {
                while (ix1 < a.size() && isdigit((unsigned char)a[ix1])) ix1++;
                while (ix2 < b.size() && isdigit((unsigned char)b[ix2])) ix2++;

                std::string seg1 = a.substr(start1, ix1 - start1);
                std::string seg2 = b.substr(start2, ix2 - start2);

                size_t z1 = seg1.find_first_not_of('0');
                size_t z2 = seg2.find_first_not_of('0');
                std::string s1 = (z1 == std::string::npos) ? "0" : seg1.substr(z1);
                std::string s2 = (z2 == std::string::npos) ? "0" : seg2.substr(z2);

                if (s1.size() < s2.size()) return -1;
                if (s1.size() > s2.size()) return 1;
                int cmp = s1.compare(s2);
                if (cmp != 0) return cmp;
            } else {
                while (ix1 < a.size() && isalpha((unsigned char)a[ix1])) ix1++;
                while (ix2 < b.size() && isalpha((unsigned char)b[ix2])) ix2++;

                std::string seg1 = a.substr(start1, ix1 - start1);
                std::string seg2 = b.substr(start2, ix2 - start2);

                int cmp = seg1.compare(seg2);
                if (cmp != 0) return cmp;
            }
        }

        if (ix1 >= a.size() && ix2 >= b.size()) return 0;
        if (ix1 < a.size()) {
            return (a[ix1] == '~') ? -1 : 1;
        }
        return (b[ix2] == '~') ? 1 : -1;
    }
};

// ============================================================================
// 6. RPM DATA STRUCTURES & MODERN HEADER PARSER
// ============================================================================
struct FileRecord {
    std::string path;
    std::string sha256;
    uint32_t size = 0;
    uint32_t mode = 0;
};

struct Dependency {
    std::string name;
    std::string version;
    uint32_t flags = 0;
};

struct RpmMetadata {
    std::string name;
    std::string version;
    std::string release;
    std::string epoch = "0";
    std::string summary;
    std::string description;
    std::string vendor;
    std::string license;
    std::string group;
    std::string url;
    std::string os;
    std::string arch;
    std::string payloadCompressor = "gzip";
    std::string headerSha256;
    uint32_t size = 0;
    uint32_t buildTime = 0;
    std::string preInScript;
    std::string postInScript;
    std::string preUnScript;
    std::string postUnScript;
    std::vector<FileRecord> files;
    std::vector<Dependency> requirements;
    std::vector<Dependency> provides;
};

class RpmPackage {
public:
    RpmMetadata meta;
    std::streampos payloadOffset = 0;
    fs::path sourcePath;

    bool parse(const fs::path& rpmPath) {
        sourcePath = rpmPath;
        std::ifstream file(rpmPath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "error: open of " << rpmPath.string() << " failed: No such file\n";
            return false;
        }

        RpmLeadRaw lead{};
        file.read(reinterpret_cast<char*>(&lead), sizeof(lead));
        if (file.gcount() != sizeof(lead) || swap32(lead.magic) != RPM_MAGIC_LEAD) {
            std::cerr << "error: " << rpmPath.string() << " is not an RPM package\n";
            return false;
        }

        if (!parseHeader(file, true)) return false;
        if (!parseHeader(file, false)) return false;

        payloadOffset = file.tellg();
        return true;
    }

private:
    bool parseHeader(std::ifstream& file, bool isSignature) {
        uint8_t magic[4] = {0};
        file.read(reinterpret_cast<char*>(magic), 4);
        if (magic[0] != 0x8E || magic[1] != 0xAD || magic[2] != 0xE8 || magic[3] != 0x01) {
            return false;
        }

        file.seekg(4, std::ios::cur);
        uint32_t nindex = 0, nbytes = 0;
        file.read(reinterpret_cast<char*>(&nindex), 4);
        file.read(reinterpret_cast<char*>(&nbytes), 4);
        nindex = swap32(nindex);
        nbytes = swap32(nbytes);

        if (nindex > 100000 || nbytes > 100000000) {
            return false;
        }

        std::vector<RpmIndexEntryRaw> entries(nindex);
        if (nindex > 0) {
            file.read(reinterpret_cast<char*>(entries.data()), nindex * sizeof(RpmIndexEntryRaw));
            if (file.gcount() != static_cast<std::streamsize>(nindex * sizeof(RpmIndexEntryRaw))) {
                return false;
            }
        }

        std::vector<char> store(nbytes);
        if (nbytes > 0) {
            file.read(store.data(), nbytes);
            if (file.gcount() != static_cast<std::streamsize>(nbytes)) {
                return false;
            }
        }

        auto getString = [&](uint32_t off) -> std::string {
            if (off >= nbytes) return "";
            size_t len = 0;
            while (off + len < nbytes && store[off + len] != '\0') ++len;
            return std::string(store.data() + off, len);
        };

        auto getStringArray = [&](uint32_t off, uint32_t count) -> std::vector<std::string> {
            std::vector<std::string> arr;
            size_t curr = off;
            for (uint32_t i = 0; i < count && curr < nbytes; ++i) {
                size_t len = 0;
                while (curr + len < nbytes && store[curr + len] != '\0') ++len;
                arr.emplace_back(store.data() + curr, len);
                curr += len + 1;
            }
            return arr;
        };

        auto getUint32Array = [&](uint32_t off, uint32_t count) -> std::vector<uint32_t> {
            std::vector<uint32_t> arr;
            if (off + (size_t)count * 4 > nbytes) return arr;
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t v = 0;
                std::memcpy(&v, store.data() + off + i * 4, 4);
                arr.push_back(swap32(v));
            }
            return arr;
        };

        auto getUint16Array = [&](uint32_t off, uint32_t count) -> std::vector<uint16_t> {
            std::vector<uint16_t> arr;
            if (off + (size_t)count * 2 > nbytes) return arr;
            for (uint32_t i = 0; i < count; ++i) {
                uint16_t v = 0;
                std::memcpy(&v, store.data() + off + i * 2, 2);
                arr.push_back(swap16(v));
            }
            return arr;
        };

        if (isSignature) {
            for (const auto& e : entries) {
                uint32_t tag = swap32(e.tag);
                uint32_t offset = swap32(e.offset);
                if (tag == RPMTAG_SHA256HEADER) {
                    meta.headerSha256 = getString(offset);
                }
            }
            std::streampos pos = file.tellg();
            if (pos % 8 != 0) file.seekg(8 - (pos % 8), std::ios::cur);
            return true;
        }

        std::vector<std::string> dirNames, baseNames, fileDigests, oldFileNames;
        std::vector<std::string> reqNames, reqVersions, provNames, provVersions;
        std::vector<uint32_t> dirIndexes, fileSizes, reqFlags, provFlags;
        std::vector<uint32_t> fileModes;

        for (const auto& entry : entries) {
            uint32_t tag = swap32(entry.tag);
            uint32_t offset = swap32(entry.offset);
            uint32_t count = swap32(entry.count);
            uint32_t type = swap32(entry.type);

            if (offset >= nbytes) continue;

            switch (tag) {
                case RPMTAG_NAME:              meta.name = getString(offset); break;
                case RPMTAG_VERSION:           meta.version = getString(offset); break;
                case RPMTAG_RELEASE:           meta.release = getString(offset); break;
                case RPMTAG_EPOCH: {
                    if (type == RPM_STRING_TYPE || type == RPM_I18NSTRING_TYPE) {
                        meta.epoch = getString(offset);
                    } else if (type == RPM_INT16_TYPE && offset + 2 <= nbytes) {
                        uint16_t ep = 0;
                        std::memcpy(&ep, store.data() + offset, 2);
                        meta.epoch = std::to_string(swap16(ep));
                    } else if (offset + 4 <= nbytes) {
                        uint32_t ep = 0;
                        std::memcpy(&ep, store.data() + offset, 4);
                        meta.epoch = std::to_string(swap32(ep));
                    }
                    break;
                }
                case RPMTAG_SUMMARY:           meta.summary = getString(offset); break;
                case RPMTAG_DESCRIPTION:       meta.description = getString(offset); break;
                case RPMTAG_VENDOR:            meta.vendor = getString(offset); break;
                case RPMTAG_LICENSE:           meta.license = getString(offset); break;
                case RPMTAG_GROUP:             meta.group = getString(offset); break;
                case RPMTAG_URL:               meta.url = getString(offset); break;
                case RPMTAG_OS:                meta.os = getString(offset); break;
                case RPMTAG_ARCH:              meta.arch = getString(offset); break;
                case RPMTAG_PAYLOADCOMPRESSOR: meta.payloadCompressor = getString(offset); break;
                case RPMTAG_PREIN:             meta.preInScript = getString(offset); break;
                case RPMTAG_POSTIN:            meta.postInScript = getString(offset); break;
                case RPMTAG_PREUN:             meta.preUnScript = getString(offset); break;
                case RPMTAG_POSTUN:            meta.postUnScript = getString(offset); break;
                case RPMTAG_SIZE: {
                    if (offset + 4 <= nbytes) {
                        uint32_t sz = 0;
                        std::memcpy(&sz, store.data() + offset, 4);
                        meta.size = swap32(sz);
                    }
                    break;
                }
                case RPMTAG_BUILDTIME: {
                    if (offset + 4 <= nbytes) {
                        uint32_t bt = 0;
                        std::memcpy(&bt, store.data() + offset, 4);
                        meta.buildTime = swap32(bt);
                    }
                    break;
                }
                case RPMTAG_DIRNAMES:          dirNames = getStringArray(offset, count); break;
                case RPMTAG_BASENAMES:         baseNames = getStringArray(offset, count); break;
                case RPMTAG_DIRINDEXES:        dirIndexes = getUint32Array(offset, count); break;
                case RPMTAG_FILESIZES:         fileSizes = getUint32Array(offset, count); break;
                case RPMTAG_FILEMODES: {
                    if (type == RPM_INT32_TYPE) {
                        fileModes = getUint32Array(offset, count);
                    } else {
                        std::vector<uint16_t> m16 = getUint16Array(offset, count);
                        fileModes.assign(m16.begin(), m16.end());
                    }
                    break;
                }
                case RPMTAG_FILEDIGESTS:       fileDigests = getStringArray(offset, count); break;
                case RPMTAG_OLDFILENAMES:      oldFileNames = getStringArray(offset, count); break;
                case RPMTAG_REQUIRENAME:       reqNames = getStringArray(offset, count); break;
                case RPMTAG_REQUIREVERSION:    reqVersions = getStringArray(offset, count); break;
                case RPMTAG_REQUIREFLAGS:      reqFlags = getUint32Array(offset, count); break;
                case RPMTAG_PROVIDENAME:       provNames = getStringArray(offset, count); break;
                case RPMTAG_PROVIDEVERSION:    provVersions = getStringArray(offset, count); break;
                case RPMTAG_PROVIDEFLAGS:      provFlags = getUint32Array(offset, count); break;
                default: break;
            }
        }

        // Modern Triple-Tag Path Reconstruction
        if (!baseNames.empty() && !dirNames.empty() && !dirIndexes.empty()) {
            for (size_t i = 0; i < baseNames.size(); ++i) {
                FileRecord rec;
                uint32_t dIdx = (i < dirIndexes.size()) ? dirIndexes[i] : 0;
                std::string dir = (dIdx < dirNames.size()) ? dirNames[dIdx] : "";
                rec.path = dir + baseNames[i];
                rec.size = (i < fileSizes.size()) ? fileSizes[i] : 0;
                rec.mode = (i < fileModes.size()) ? fileModes[i] : 0644;
                rec.sha256 = (i < fileDigests.size()) ? fileDigests[i] : "";
                meta.files.push_back(rec);
            }
        } else if (!oldFileNames.empty()) {
            for (size_t i = 0; i < oldFileNames.size(); ++i) {
                FileRecord rec;
                rec.path = oldFileNames[i];
                rec.size = (i < fileSizes.size()) ? fileSizes[i] : 0;
                rec.mode = (i < fileModes.size()) ? fileModes[i] : 0644;
                rec.sha256 = (i < fileDigests.size()) ? fileDigests[i] : "";
                meta.files.push_back(rec);
            }
        }

        for (size_t i = 0; i < reqNames.size(); ++i) {
            Dependency dep;
            dep.name = reqNames[i];
            dep.version = (i < reqVersions.size()) ? reqVersions[i] : "";
            dep.flags = (i < reqFlags.size()) ? reqFlags[i] : 0;
            meta.requirements.push_back(dep);
        }

        for (size_t i = 0; i < provNames.size(); ++i) {
            Dependency dep;
            dep.name = provNames[i];
            dep.version = (i < provVersions.size()) ? provVersions[i] : "";
            dep.flags = (i < provFlags.size()) ? provFlags[i] : 0;
            meta.provides.push_back(dep);
        }

        return true;
    }
};

// ============================================================================
// 7. PERSISTENT ON-DISK WRITE-AHEAD JOURNAL (CRASH RECOVERY)
// ============================================================================
enum class WalAction : uint32_t { FileCreated = 1, FileReplaced = 2 };

class PersistentWalJournal {
private:
    fs::path journalDir;
    fs::path activeWalFile;
    fs::path stagingDir;
    std::ofstream walStream;
    std::vector<std::pair<WalAction, std::pair<fs::path, fs::path>>> loggedActions;

public:
    PersistentWalJournal(const fs::path& root = "") {
        if (!root.empty() && root != "C:\\" && root != "C:/") {
            journalDir = root / "ProgramData" / "RPM" / "journal";
        } else {
            char path[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path))) {
                journalDir = fs::path(path) / "RPM" / "journal";
            } else {
                journalDir = "C:\\ProgramData\\RPM\\journal";
            }
        }
        std::error_code ec;
        fs::create_directories(journalDir, ec);
        stagingDir = journalDir / ("tx_" + std::to_string(GetCurrentProcessId()));
        fs::create_directories(stagingDir, ec);

        activeWalFile = journalDir / ("tx_" + std::to_string(GetCurrentProcessId()) + ".wal");
        walStream.open(activeWalFile, std::ios::out | std::ios::app);
    }

    ~PersistentWalJournal() {
        if (walStream.is_open()) walStream.close();
        std::error_code ec;
        fs::remove(activeWalFile, ec);
        fs::remove_all(stagingDir, ec);
    }

    static void recoverInterruptedTransactions() {
        char path[MAX_PATH];
        fs::path jDir = "C:\\ProgramData\\RPM\\journal";
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path))) {
            jDir = fs::path(path) / "RPM" / "journal";
        }
        std::error_code ec;
        if (!fs::exists(jDir, ec)) return;

        for (const auto& entry : fs::directory_iterator(jDir, ec)) {
            if (entry.path().extension() == ".wal") {
                std::string fname = entry.path().stem().string();
                if (fname.rfind("tx_", 0) == 0) {
                    try {
                        DWORD pid = std::stoul(fname.substr(3));
                        if (pid > 0) {
                            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                            if (hProc != NULL) {
                                DWORD exitCode = 0;
                                if (GetExitCodeProcess(hProc, &exitCode) && exitCode == STILL_ACTIVE) {
                                    CloseHandle(hProc);
                                    continue; // Process is still actively running
                                }
                                CloseHandle(hProc);
                            }
                        }
                    } catch (...) {}
                }

                std::cerr << "rpm: Interrupted transaction found: " << entry.path().filename().string()
                          << ". Performing crash recovery rollback...\n";
                std::ifstream wal(entry.path());
                std::string line;
                std::vector<std::string> ops;
                while (std::getline(wal, line)) {
                    if (!line.empty()) ops.push_back(line);
                }
                wal.close();

                for (auto it = ops.rbegin(); it != ops.rend(); ++it) {
                    auto sep = it->find(':');
                    if (sep == std::string::npos) continue;
                    std::string act = it->substr(0, sep);
                    std::string payload = it->substr(sep + 1);

                    if (act == "CREATE") {
                        fs::remove(payload, ec);
                    } else if (act == "REPLACE") {
                        auto bar = payload.find('|');
                        if (bar != std::string::npos) {
                            fs::path target = payload.substr(0, bar);
                            fs::path backup = payload.substr(bar + 1);
                            if (fs::exists(backup, ec)) {
                                fs::copy_file(backup, target, fs::copy_options::overwrite_existing, ec);
                            }
                        }
                    }
                }
                fs::remove(entry.path(), ec);
                fs::path stg = entry.path().parent_path() / entry.path().stem();
                fs::remove_all(stg, ec);
            }
        }
    }

    bool safeWrite(const fs::path& dest, const std::vector<char>& data, uint32_t posixMode, std::string* outSha256 = nullptr) {
        std::error_code ec;
        if (fs::exists(dest, ec)) {
            fs::path backup = stagingDir / ("bak_" + std::to_string(loggedActions.size()));
            fs::copy_file(dest, backup, fs::copy_options::overwrite_existing, ec);
            walStream << "REPLACE:" << dest.string() << "|" << backup.string() << "\n";
            walStream.flush();
            loggedActions.push_back({WalAction::FileReplaced, {dest, backup}});
        } else {
            if (dest.has_parent_path()) fs::create_directories(dest.parent_path(), ec);
            walStream << "CREATE:" << dest.string() << "\n";
            walStream.flush();
            loggedActions.push_back({WalAction::FileCreated, {dest, ""}});
        }

        HANDLE hFile = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (err == ERROR_SHARING_VIOLATION || err == ERROR_ACCESS_DENIED) {
                fs::path pending = dest.string() + ".rpmsave";
                std::ofstream out(pending, std::ios::binary);
                if (out.is_open()) {
                    out.write(data.data(), data.size());
                    out.close();
                    if (outSha256) {
                        *outSha256 = CngCryptoEngine::calculateFileSha256(pending);
                    }
                    MoveFileExW(pending.c_str(), dest.c_str(), MOVEFILE_DELAY_UNTIL_REBOOT | MOVEFILE_REPLACE_EXISTING);
                    return true;
                }
            }
            return false;
        }

        DWORD written = 0;
        BOOL ok = WriteFile(hFile, data.data(), (DWORD)data.size(), &written, NULL);
        CloseHandle(hFile);

        if (ok && (written == data.size())) {
            SecurityEngine::applyPosixPermissions(dest, posixMode);
            if (outSha256) {
                *outSha256 = CngCryptoEngine::calculateFileSha256(dest);
            }
            return true;
        }
        return false;
    }

    void rollback() {
        if (walStream.is_open()) walStream.close();
        std::error_code ec;
        for (auto it = loggedActions.rbegin(); it != loggedActions.rend(); ++it) {
            if (it->first == WalAction::FileCreated) {
                fs::remove(it->second.first, ec);
            } else if (it->first == WalAction::FileReplaced) {
                if (fs::exists(it->second.second, ec)) {
                    fs::copy_file(it->second.second, it->second.first, fs::copy_options::overwrite_existing, ec);
                }
            }
        }
        loggedActions.clear();
        fs::remove(activeWalFile, ec);
        fs::remove_all(stagingDir, ec);
    }

    void commit() {
        if (walStream.is_open()) walStream.close();
        loggedActions.clear();
        std::error_code ec;
        fs::remove(activeWalFile, ec);
        fs::remove_all(stagingDir, ec);
    }
};

// ============================================================================
// 8. ARCHIVE EXTRACTION ENGINE
// ============================================================================
class ArchiveExtractionEngine {
private:
    static fs::path sanitizePath(const fs::path& root, const std::string& raw) {
        std::string s = raw;
        std::replace(s.begin(), s.end(), '/', '\\');

        // Remove drive specifiers if any
        if (s.size() >= 2 && s[1] == ':') {
            s = s.substr(2);
        }

        // Strip leading slashes and relative current directory prefix
        while (!s.empty() && (s[0] == '\\' || s[0] == '/')) s = s.substr(1);
        if (s.rfind(".\\", 0) == 0) s = s.substr(2);

        // Disallow path traversal sequences
        fs::path relPath;
        std::istringstream iss(s);
        std::string token;
        while (std::getline(iss, token, '\\')) {
            if (token.empty() || token == ".") continue;
            if (token == "..") continue; // Prevent directory traversal
            relPath /= token;
        }

        return root / relPath;
    }

public:
    static bool extract(const fs::path& rpmPath, std::streampos offset, const fs::path& root,
                        PersistentWalJournal& journal, std::vector<FileRecord>& outFiles,
                        const std::vector<FileRecord>& headerFiles) {
        std::ifstream file(rpmPath, std::ios::binary);
        if (!file.is_open()) return false;

        file.seekg(0, std::ios::end);
        std::streampos endPos = file.tellg();
        if (endPos <= offset) return false;

        size_t totalSize = static_cast<size_t>(endPos - offset);
        file.seekg(offset);

        std::vector<char> buffer(totalSize);
        file.read(buffer.data(), totalSize);

#if defined(USE_LIBARCHIVE)
        struct archive* a = archive_read_new();
        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);

        if (archive_read_open_memory(a, buffer.data(), buffer.size()) != ARCHIVE_OK) {
            archive_read_free(a);
            return false;
        }

        struct archive_entry* entry;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            std::string pathStr = archive_entry_pathname(entry);
            fs::path targetPath = sanitizePath(root, pathStr);
            size_t size = archive_entry_size(entry);
            mode_t mode = archive_entry_mode(entry);

            if (archive_entry_filetype(entry) == AE_IFDIR) {
                std::error_code ec;
                fs::create_directories(targetPath, ec);
            } else {
                std::vector<char> fileData(size);
                archive_read_data(a, fileData.data(), size);
                std::string computedHash;
                if (!journal.safeWrite(targetPath, fileData, static_cast<uint32_t>(mode), &computedHash)) {
                    archive_read_free(a);
                    return false;
                }
                FileRecord rec;
                rec.path = targetPath.string();
                rec.size = static_cast<uint32_t>(size);
                rec.mode = static_cast<uint32_t>(mode);
                rec.sha256 = computedHash;
                outFiles.push_back(rec);
            }
        }
        archive_read_free(a);
        return true;
#else
        // Check if uncompressed CPIO
        bool isUncompressedCpio = false;
        if (buffer.size() >= 6) {
            if (std::memcmp(buffer.data(), "070701", 6) == 0 || std::memcmp(buffer.data(), "070702", 6) == 0) {
                isUncompressedCpio = true;
            }
        }

        if (isUncompressedCpio) {
            #pragma pack(push, 1)
            struct CpioNewc {
                char magic[6]; char ino[8]; char mode[8]; char uid[8]; char gid[8];
                char nlink[8]; char mtime[8]; char filesize[8]; char devmajor[8];
                char devminor[8]; char rdevmajor[8]; char rdevminor[8];
                char namesize[8]; char check[8];
            };
            #pragma pack(pop)

            auto parseHex = [](const char* p, size_t n) -> uint32_t {
                std::string s(p, n);
                return static_cast<uint32_t>(std::strtoul(s.c_str(), nullptr, 16));
            };

            const char* ptr = buffer.data();
            const char* end = ptr + buffer.size();
            bool trailerFound = false;

            while (ptr + sizeof(CpioNewc) <= end) {
                const CpioNewc* hdr = reinterpret_cast<const CpioNewc*>(ptr);
                if (std::memcmp(hdr->magic, "070701", 6) != 0 && std::memcmp(hdr->magic, "070702", 6) != 0) {
                    break;
                }

                uint32_t mode     = parseHex(hdr->mode, 8);
                uint32_t filesize = parseHex(hdr->filesize, 8);
                uint32_t namesize = parseHex(hdr->namesize, 8);

                ptr += sizeof(CpioNewc);
                if (ptr + namesize > end) return false;

                std::string filename(ptr, namesize > 0 ? namesize - 1 : 0);
                ptr += namesize;

                size_t padName = (4 - ((sizeof(CpioNewc) + namesize) % 4)) % 4;
                ptr += padName;

                if (filename == "TRAILER!!!") {
                    trailerFound = true;
                    break;
                }
                if (filename.empty()) continue;

                fs::path dest = sanitizePath(root, filename);
                bool isDir = (mode & 0040000) != 0;

                if (isDir) {
                    std::error_code ec;
                    fs::create_directories(dest, ec);
                } else {
                    if (ptr + filesize > end) return false;
                    std::vector<char> fileData(ptr, ptr + filesize);
                    ptr += filesize;

                    std::string computedHash;
                    if (!journal.safeWrite(dest, fileData, mode, &computedHash)) return false;

                    FileRecord rec;
                    rec.path = dest.string();
                    rec.size = filesize;
                    rec.mode = mode;
                    rec.sha256 = computedHash;
                    outFiles.push_back(rec);
                }

                size_t padData = (4 - (filesize % 4)) % 4;
                ptr += padData;
            }
            if (!trailerFound && !headerFiles.empty() && outFiles.empty()) {
                return false;
            }
            return true;
        }

        // Compressed payload (gzip/xz/zstd/bzip2) fallback using Windows native tar.exe (bsdtar)
        fs::path tempPayload = fs::temp_directory_path() / ("rpm_payload_" + std::to_string(GetCurrentProcessId()) + ".bin");
        fs::path tempExtract = fs::temp_directory_path() / ("rpm_extract_" + std::to_string(GetCurrentProcessId()));

        {
            std::ofstream out(tempPayload, std::ios::binary);
            if (!out.is_open()) return false;
            out.write(buffer.data(), buffer.size());
        }

        std::error_code ec;
        fs::create_directories(tempExtract, ec);

        std::wstring tarCmd = L"tar.exe -xf \"" + tempPayload.wstring() + L"\" -C \"" + tempExtract.wstring() + L"\"";
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        std::vector<wchar_t> cmdBuf(tarCmd.begin(), tarCmd.end());
        cmdBuf.push_back(L'\0');

        BOOL ok = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        bool tarSuccess = false;
        if (ok) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD exitCode = 1;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            if (exitCode == 0) {
                tarSuccess = true;
                for (const auto& entry : fs::recursive_directory_iterator(tempExtract, ec)) {
                    fs::path rel = fs::relative(entry.path(), tempExtract, ec);
                    fs::path dest = sanitizePath(root, rel.string());

                    if (entry.is_directory(ec)) {
                        fs::create_directories(dest, ec);
                    } else if (entry.is_regular_file(ec)) {
                        std::ifstream in(entry.path(), std::ios::binary);
                        std::vector<char> fileData((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                        in.close();

                        // Look up true mode from headerFiles if available
                        uint32_t mode = 0100644;
                        std::string relGen = rel.generic_string();
                        for (const auto& hf : headerFiles) {
                            fs::path hfPath(hf.path);
                            if (hfPath.generic_string() == relGen ||
                                hfPath.filename() == rel.filename() ||
                                hf.path.find(relGen) != std::string::npos) {
                                mode = hf.mode;
                                break;
                            }
                        }
                        if (mode == 0) mode = 0100644;

                        std::string computedHash;
                        if (!journal.safeWrite(dest, fileData, mode, &computedHash)) {
                            fs::remove(tempPayload, ec);
                            fs::remove_all(tempExtract, ec);
                            return false;
                        }

                        FileRecord rec;
                        rec.path = dest.string();
                        rec.size = static_cast<uint32_t>(fileData.size());
                        rec.mode = mode;
                        rec.sha256 = computedHash;
                        outFiles.push_back(rec);
                    }
                }
            }
        }

        fs::remove(tempPayload, ec);
        fs::remove_all(tempExtract, ec);

        if (!tarSuccess) {
            return false;
        }

        if (outFiles.empty() && !headerFiles.empty()) {
            return false;
        }
        return true;
#endif
    }
};

// ============================================================================
// 9. RPM DATABASE ENGINE (%ProgramData%\RPM)
// ============================================================================
class RpmDatabase {
private:
    fs::path dbDir;
    fs::path manifestPath;
    fs::path lockPath;
    HANDLE hLock = INVALID_HANDLE_VALUE;
    std::map<std::string, RpmMetadata> packages;

public:
    RpmDatabase(const fs::path& root = "") {
        if (!root.empty() && root != "C:\\" && root != "C:/") {
            dbDir = root / "ProgramData" / "RPM";
        } else {
            char path[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path))) {
                dbDir = fs::path(path) / "RPM";
            } else {
                dbDir = "C:\\ProgramData\\RPM";
            }
        }
        std::error_code ec;
        fs::create_directories(dbDir, ec);
        manifestPath = dbDir / "rpmdb.json";
        lockPath = dbDir / ".rpmdb.lock";
    }

    bool lock() {
        std::error_code ec;
        fs::create_directories(dbDir, ec);
        hLock = CreateFileW(lockPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hLock == INVALID_HANDLE_VALUE) return false;
        OVERLAPPED ov = {0};
        if (!LockFileEx(hLock, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &ov)) {
            CloseHandle(hLock);
            hLock = INVALID_HANDLE_VALUE;
            return false;
        }
        return true;
    }

    void unlock() {
        if (hLock != INVALID_HANDLE_VALUE) {
            OVERLAPPED ov = {0};
            UnlockFileEx(hLock, 0, MAXDWORD, MAXDWORD, &ov);
            CloseHandle(hLock);
            hLock = INVALID_HANDLE_VALUE;
        }
    }

    void load() {
        packages.clear();
        std::error_code ec;
        if (!fs::exists(manifestPath, ec)) return;

        std::ifstream file(manifestPath);
        if (!file.is_open()) return;

        std::string line;
        RpmMetadata curr;
        bool inPkg = false;

        while (std::getline(file, line)) {
            if (line.empty()) continue;
            if (line == "PKG_START") {
                curr = RpmMetadata();
                inPkg = true;
            } else if (line == "PKG_END") {
                if (inPkg && !curr.name.empty()) packages[curr.name] = curr;
                inPkg = false;
            } else if (inPkg) {
                auto eq = line.find('=');
                if (eq == std::string::npos) continue;
                std::string k = line.substr(0, eq);
                std::string v = line.substr(eq + 1);

                if (k == "name") curr.name = v;
                else if (k == "version") curr.version = v;
                else if (k == "release") curr.release = v;
                else if (k == "epoch") curr.epoch = v;
                else if (k == "arch") curr.arch = v;
                else if (k == "summary") curr.summary = base64Decode(v);
                else if (k == "description") curr.description = base64Decode(v);
                else if (k == "vendor") curr.vendor = base64Decode(v);
                else if (k == "license") curr.license = v;
                else if (k == "group") curr.group = v;
                else if (k == "url") curr.url = v;
                else if (k == "os") curr.os = v;
                else if (k == "prein") curr.preInScript = base64Decode(v);
                else if (k == "postin") curr.postInScript = base64Decode(v);
                else if (k == "preun") curr.preUnScript = base64Decode(v);
                else if (k == "postun") curr.postUnScript = base64Decode(v);
                else if (k == "size") {
                    try { curr.size = static_cast<uint32_t>(std::stoul(v)); } catch (...) {}
                } else if (k == "buildtime") {
                    try { curr.buildTime = static_cast<uint32_t>(std::stoul(v)); } catch (...) {}
                } else if (k == "file") {
                    auto c1 = v.find('|');
                    if (c1 != std::string::npos) {
                        auto c2 = v.find('|', c1 + 1);
                        if (c2 != std::string::npos) {
                            auto c3 = v.find('|', c2 + 1);
                            FileRecord fr;
                            fr.path = v.substr(0, c1);
                            fr.sha256 = v.substr(c1 + 1, c2 - c1 - 1);
                            try {
                                if (c3 != std::string::npos) {
                                    fr.size = static_cast<uint32_t>(std::stoul(v.substr(c2 + 1, c3 - c2 - 1)));
                                    fr.mode = static_cast<uint32_t>(std::stoul(v.substr(c3 + 1)));
                                } else {
                                    fr.size = static_cast<uint32_t>(std::stoul(v.substr(c2 + 1)));
                                    fr.mode = 0644;
                                }
                            } catch (...) {}
                            curr.files.push_back(fr);
                        }
                    }
                } else if (k == "require") {
                    auto c1 = v.find('|');
                    if (c1 != std::string::npos) {
                        auto c2 = v.find('|', c1 + 1);
                        if (c2 != std::string::npos) {
                            Dependency dep;
                            dep.name = v.substr(0, c1);
                            dep.version = v.substr(c1 + 1, c2 - c1 - 1);
                            try { dep.flags = static_cast<uint32_t>(std::stoul(v.substr(c2 + 1))); } catch (...) {}
                            curr.requirements.push_back(dep);
                        }
                    }
                } else if (k == "provide") {
                    auto c1 = v.find('|');
                    if (c1 != std::string::npos) {
                        auto c2 = v.find('|', c1 + 1);
                        if (c2 != std::string::npos) {
                            Dependency dep;
                            dep.name = v.substr(0, c1);
                            dep.version = v.substr(c1 + 1, c2 - c1 - 1);
                            try { dep.flags = static_cast<uint32_t>(std::stoul(v.substr(c2 + 1))); } catch (...) {}
                            curr.provides.push_back(dep);
                        }
                    }
                }
            }
        }
    }

    void save() {
        fs::path tmp = manifestPath.string() + ".tmp";
        std::ofstream file(tmp, std::ios::trunc);
        for (const auto& [name, pkg] : packages) {
            file << "PKG_START\n";
            file << "name=" << pkg.name << "\n";
            file << "version=" << pkg.version << "\n";
            file << "release=" << pkg.release << "\n";
            file << "epoch=" << pkg.epoch << "\n";
            file << "arch=" << pkg.arch << "\n";
            file << "summary=" << base64Encode(pkg.summary) << "\n";
            file << "description=" << base64Encode(pkg.description) << "\n";
            file << "vendor=" << base64Encode(pkg.vendor) << "\n";
            file << "license=" << pkg.license << "\n";
            file << "group=" << pkg.group << "\n";
            file << "url=" << pkg.url << "\n";
            file << "os=" << pkg.os << "\n";
            file << "prein=" << base64Encode(pkg.preInScript) << "\n";
            file << "postin=" << base64Encode(pkg.postInScript) << "\n";
            file << "preun=" << base64Encode(pkg.preUnScript) << "\n";
            file << "postun=" << base64Encode(pkg.postUnScript) << "\n";
            file << "size=" << pkg.size << "\n";
            file << "buildtime=" << pkg.buildTime << "\n";

            for (const auto& f : pkg.files) {
                file << "file=" << f.path << "|" << f.sha256 << "|" << f.size << "|" << f.mode << "\n";
            }
            for (const auto& req : pkg.requirements) {
                file << "require=" << req.name << "|" << req.version << "|" << req.flags << "\n";
            }
            for (const auto& prov : pkg.provides) {
                file << "provide=" << prov.name << "|" << prov.version << "|" << prov.flags << "\n";
            }
            file << "PKG_END\n";
        }
        file.close();
        MoveFileExW(tmp.c_str(), manifestPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    }

    bool isInstalled(const std::string& name) { return packages.find(name) != packages.end(); }
    RpmMetadata* find(const std::string& name) {
        auto it = packages.find(name);
        return it != packages.end() ? &it->second : nullptr;
    }
    const std::map<std::string, RpmMetadata>& all() const { return packages; }
    void add(const RpmMetadata& pkg) { packages[pkg.name] = pkg; save(); }
    bool erase(const std::string& name) {
        if (packages.erase(name) > 0) { save(); return true; }
        return false;
    }
};

// ============================================================================
// 10. SYSTEM SCRIPTLET RUNNER
// ============================================================================
class ScriptletRunner {
public:
    static bool execute(const std::string& script, const std::string& phase) {
        if (script.empty()) return true;

        fs::path tempScript = fs::temp_directory_path() / ("rpm_" + phase + "_" + std::to_string(GetCurrentProcessId()) + ".bat");
        {
            std::ofstream out(tempScript);
            out << "@echo off\r\n" << script << "\r\n";
        }

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi{};
        std::string cmd = "cmd.exe /s /c \"\"" + tempScript.string() + "\"\"";

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
            fs::remove(tempScript, ec);
            return exitCode == 0;
        }
        fs::remove(tempScript, ec);
        return false;
    }
};

// ============================================================================
// 11. DEPENDENCY ENGINE
// ============================================================================
class DependencyEngine {
public:
    static bool matchDependency(const std::string& candidateVer, const Dependency& req) {
        if (req.version.empty() || req.flags == 0) return true;

        int cmp = EvrEngine::rpmvercmp(candidateVer, req.version);
        if (cmp == 0 && (req.flags & RPMSENSE_EQUAL)) return true;
        if (cmp > 0 && (req.flags & RPMSENSE_GREATER)) return true;
        if (cmp < 0 && (req.flags & RPMSENSE_LESS)) return true;

        return false;
    }

    static bool verify(const RpmMetadata& pkg, RpmDatabase& db) {
        for (const auto& req : pkg.requirements) {
            if (req.name.rfind("rpmlib(", 0) == 0) continue;

            bool satisfied = false;
            for (const auto& [name, installed] : db.all()) {
                // Match direct package name
                if (installed.name == req.name) {
                    if (matchDependency(installed.version, req)) {
                        satisfied = true;
                        break;
                    }
                }

                // Match provides virtual capabilities
                for (const auto& prov : installed.provides) {
                    if (prov.name == req.name) {
                        std::string provVer = prov.version.empty() ? installed.version : prov.version;
                        if (matchDependency(provVer, req)) {
                            satisfied = true;
                            break;
                        }
                    }
                }
                if (satisfied) break;
            }

            if (!satisfied) {
                std::cerr << "error: Failed dependencies:\n";
                std::cerr << "  " << req.name << " " << (req.version.empty() ? "" : "= " + req.version)
                          << " is needed by " << pkg.name << "-" << pkg.version << "-" << pkg.release << "\n";
                return false;
            }
        }
        return true;
    }

    static bool checkEraseDependencies(const std::string& pkgName, RpmDatabase& db) {
        RpmMetadata* found = db.find(pkgName);
        if (!found) return true;

        std::set<std::string> providedCaps;
        providedCaps.insert(pkgName);
        for (const auto& prov : found->provides) {
            providedCaps.insert(prov.name);
        }

        for (const auto& [name, installed] : db.all()) {
            if (installed.name == pkgName) continue;
            for (const auto& req : installed.requirements) {
                if (providedCaps.count(req.name) > 0) {
                    // Check if another installed package still provides this requirement
                    bool stillProvided = false;
                    for (const auto& [otherName, otherPkg] : db.all()) {
                        if (otherName == pkgName) continue;
                        if (otherPkg.name == req.name && matchDependency(otherPkg.version, req)) {
                            stillProvided = true;
                            break;
                        }
                        for (const auto& prov : otherPkg.provides) {
                            if (prov.name == req.name) {
                                std::string provVer = prov.version.empty() ? otherPkg.version : prov.version;
                                if (matchDependency(provVer, req)) {
                                    stillProvided = true;
                                    break;
                                }
                            }
                        }
                        if (stillProvided) break;
                    }

                    if (!stillProvided) {
                        std::cerr << "error: Failed dependencies:\n";
                        std::cerr << "  " << req.name << " is needed by (installed) "
                                  << installed.name << "-" << installed.version << "-" << installed.release << "\n";
                        return false;
                    }
                }
            }
        }
        return true;
    }
};

// ============================================================================
// 12. CLI HELP & OUTPUT FORMATTING
// ============================================================================
void printHelp() {
    std::cout <<
R"(rpm(1)                     CrossShell for UNIX Reference Manual                    rpm(1)

NAME
    rpm - query, install, upgrade, erase, and verify RPM packages

SYNOPSIS
    rpm [OPTIONS] [PACKAGE...]

DESCRIPTION
    Manages the RPM package database. Query operations inspect
    installed packages or RPM files; mutation operations install,
    upgrade, or erase packages.

OPTIONS
    Query Options:
        -q, --query
            Query package.
        -a, --all
            Query all installed packages.
        -i, --info
            Display package metadata including name, version, and summary.
        -l, --list
            List files in package.
        -p, --package
            Query an uninstalled package file.
        --whatprovides CAPABILITY
            Query package providing specific capability.
        --whatrequires CAPABILITY
            Query packages that require a capability.

    Install, Upgrade, and Erase Options:
        -i, --install
            Install a package.
        -U, --upgrade
            Upgrade a package (installs if not present).
        -e, --erase
            Erase (uninstall) a package.
        -v, --verbose
            Provide detailed progress and diagnostic output.
        -h, --hash
            Print 50 hash marks as package unpacks.
        --nodeps
            Do not verify package dependencies.
        --noscripts
            Do not execute package pre/post install scripts.
        --test
            Do not install, but tell if it would work.
        --prefix=DIR
            Relocate package to DIR if relocatable.
        --root=DIR
            Use DIR as top-level root directory.

    Verification Options:
        -V, --verify
            Verify a package installation.

    Common Options:
        -?, --help
            Display this comprehensive reference manual and exit.
        --version
            Display version information and exit.

EXAMPLES
    rpm -qa
        List all installed packages.

    rpm -qi package-name
        Display package metadata.

    rpm -ql package-name
        List files installed by a package.

    rpm -U package.rpm --test
        Validate an upgrade without extracting files.

    rpm -e package-name
        Erase an installed package.

EXIT STATUS
    0   Successful query, verification, or mutation.
    1   Invalid operation, missing package, dependency, privilege, lock,
        script, extraction, or verification failure.

    CrossShell for UNIX                                                   rpm(1)
)";
}

void printProgress(const std::string& pkgName) {
    std::cout << "\rUpdating / installing...   \n";
    std::cout << "1:" << std::setw(26) << std::left << pkgName.substr(0, 25) << " ";
    for (int i = 0; i < 50; ++i) {
        std::cout << "#";
        std::cout.flush();
        Sleep(5);
    }
    std::cout << " [100%]\n";
}

// ============================================================================
// 13. MAIN DISPATCHER
// ============================================================================
int main(int argc, char* argv[]) {
    PersistentWalJournal::recoverInterruptedTransactions();

    if (argc <= 1) {
        std::cerr << "rpm: no operation specified\nTry 'rpm --help' for more information.\n";
        return 1;
    }

    bool opQuery = false, opInstall = false, opUpgrade = false, opErase = false, opVerify = false;
    bool optInfo = false, optList = false, optAll = false, optPackage = false, optHash = false;
    bool optVerbose = false, optNoDeps = false, optNoScripts = false, optTest = false;
    bool optWhatProvides = false, optWhatRequires = false;
    fs::path targetRoot = "C:\\";
    std::vector<std::string> arguments;

    // First pass: detect main mode from arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-q" || arg == "--query" || arg.rfind("-q", 0) == 0 ||
            arg == "-a" || arg == "--all" || arg == "-l" || arg == "--list" ||
            arg == "--info" || arg == "--whatprovides" || arg == "--whatrequires") {
            opQuery = true;
        } else if (arg == "-V" || arg == "--verify" || arg.rfind("-V", 0) == 0) {
            opVerify = true;
        } else if (arg == "-e" || arg == "--erase" || arg.rfind("-e", 0) == 0) {
            opErase = true;
        } else if (arg == "-U" || arg == "--upgrade" || arg.rfind("-U", 0) == 0) {
            opUpgrade = true;
        }
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-?") {
            printHelp();
            return 0;
        } else if (arg == "--version") {
            std::cout << "RPM version 3.8.19\n";
            return 0;
        } else if (arg == "--root" && i + 1 < argc) {
            targetRoot = argv[++i];
        } else if (arg.rfind("--root=", 0) == 0) {
            targetRoot = arg.substr(7);
        } else if (arg == "--prefix" && i + 1 < argc) {
            targetRoot = argv[++i];
        } else if (arg.rfind("--prefix=", 0) == 0) {
            targetRoot = arg.substr(9);
        } else if (arg == "--nodeps") {
            optNoDeps = true;
        } else if (arg == "--noscripts") {
            optNoScripts = true;
        } else if (arg == "--test") {
            optTest = true;
        } else if (arg == "--whatprovides") {
            opQuery = true;
            optWhatProvides = true;
        } else if (arg == "--whatrequires") {
            opQuery = true;
            optWhatRequires = true;
        } else if (arg.rfind("-", 0) == 0 && arg.rfind("--", 0) != 0) {
            for (size_t c = 1; c < arg.size(); ++c) {
                switch (arg[c]) {
                    case 'q': opQuery = true; break;
                    case 'i':
                        if (opQuery) optInfo = true;
                        else opInstall = true;
                        break;
                    case 'U': opUpgrade = true; break;
                    case 'e': opErase = true; break;
                    case 'V': opVerify = true; break;
                    case 'a': opQuery = true; optAll = true; break;
                    case 'l': opQuery = true; optList = true; break;
                    case 'p': optPackage = true; break;
                    case 'h': optHash = true; break;
                    case 'v': optVerbose = true; break;
                    default:
                        std::cerr << "rpm: invalid option -- '" << arg[c] << "'\n";
                        return 1;
                }
            }
        } else if (arg == "--query") opQuery = true;
        else if (arg == "--install") opInstall = true;
        else if (arg == "--upgrade") opUpgrade = true;
        else if (arg == "--erase") opErase = true;
        else if (arg == "--verify") opVerify = true;
        else if (arg == "--info") { opQuery = true; optInfo = true; }
        else if (arg == "--list") { opQuery = true; optList = true; }
        else if (arg == "--all") { opQuery = true; optAll = true; }
        else if (arg == "--package") optPackage = true;
        else if (arg == "--hash") optHash = true;
        else if (arg == "--verbose") optVerbose = true;
        else arguments.push_back(arg);
    }

    RpmDatabase db(targetRoot);

    // ========================================================================
    // QUERY OPERATIONS
    // ========================================================================
    if (opQuery) {
        db.load();

        if (optWhatProvides) {
            if (arguments.empty()) {
                std::cerr << "error: no capability specified for --whatprovides\n";
                return 1;
            }
            int queryExitCode = 0;
            for (const auto& cap : arguments) {
                bool foundAny = false;
                for (const auto& [name, pkg] : db.all()) {
                    if (pkg.name == cap) {
                        std::cout << pkg.name << "-" << pkg.version << "-" << pkg.release << "." << pkg.arch << "\n";
                        foundAny = true;
                        continue;
                    }
                    for (const auto& prov : pkg.provides) {
                        if (prov.name == cap) {
                            std::cout << pkg.name << "-" << pkg.version << "-" << pkg.release << "." << pkg.arch << "\n";
                            foundAny = true;
                            break;
                        }
                    }
                }
                if (!foundAny) {
                    std::cerr << "no package provides " << cap << "\n";
                    queryExitCode = 1;
                }
            }
            return queryExitCode;
        }

        if (optWhatRequires) {
            if (arguments.empty()) {
                std::cerr << "error: no capability specified for --whatrequires\n";
                return 1;
            }
            int queryExitCode = 0;
            for (const auto& cap : arguments) {
                bool foundAny = false;
                for (const auto& [name, pkg] : db.all()) {
                    for (const auto& req : pkg.requirements) {
                        if (req.name == cap) {
                            std::cout << pkg.name << "-" << pkg.version << "-" << pkg.release << "." << pkg.arch << "\n";
                            foundAny = true;
                            break;
                        }
                    }
                }
                if (!foundAny) {
                    std::cerr << "no package requires " << cap << "\n";
                    queryExitCode = 1;
                }
            }
            return queryExitCode;
        }

        if (optAll) {
            for (const auto& [name, pkg] : db.all()) {
                std::cout << pkg.name << "-" << pkg.version << "-" << pkg.release << "." << pkg.arch << "\n";
            }
            return 0;
        }

        if (arguments.empty()) {
            std::cerr << "error: no packages given for query\n";
            return 1;
        }

        int queryExitCode = 0;
        for (const auto& target : arguments) {
            RpmMetadata meta;
            if (optPackage) {
                RpmPackage pkg;
                if (!pkg.parse(target)) return 1;
                meta = pkg.meta;
            } else {
                RpmMetadata* found = db.find(target);
                if (!found) {
                    std::cerr << "package " << target << " is not installed\n";
                    queryExitCode = 1;
                    continue;
                }
                meta = *found;
            }

            if (optInfo) {
                std::cout << std::left << std::setw(15) << "Name" << ": " << meta.name << "\n";
                std::cout << std::left << std::setw(15) << "Version" << ": " << meta.version << "\n";
                std::cout << std::left << std::setw(15) << "Release" << ": " << meta.release << "\n";
                std::cout << std::left << std::setw(15) << "Architecture" << ": " << meta.arch << "\n";
                std::cout << std::left << std::setw(15) << "Group" << ": " << meta.group << "\n";
                std::cout << std::left << std::setw(15) << "Size" << ": " << meta.size << "\n";
                std::cout << std::left << std::setw(15) << "License" << ": " << meta.license << "\n";
                std::cout << std::left << std::setw(15) << "Summary" << ": " << meta.summary << "\n";
                std::cout << std::left << std::setw(15) << "Description" << ":\n" << meta.description << "\n";
            } else if (optList) {
                for (const auto& f : meta.files) std::cout << f.path << "\n";
            } else {
                std::cout << meta.name << "-" << meta.version << "-" << meta.release << "." << meta.arch << "\n";
            }
        }
        return queryExitCode;
    }

    // ========================================================================
    // MUTATION OPERATIONS (Install / Upgrade / Erase)
    // ========================================================================
    if (opInstall || opUpgrade || opErase) {
        bool isDefaultRoot = (targetRoot == "C:\\" || targetRoot == "C:/");
        if (!optTest && isDefaultRoot && !SecurityEngine::isAdministrator()) {
            std::cerr << "error: RPM operations require administrative privileges.\n";
            return 1;
        }

        if (!optTest) {
            if (!db.lock()) {
                std::cerr << "error: failed to acquire database lock\n";
                return 1;
            }
        }
        db.load();

        if (opInstall || opUpgrade) {
            if (arguments.empty()) {
                std::cerr << "error: no packages given for install\n";
                db.unlock();
                return 1;
            }

            for (const auto& rpmFilePath : arguments) {
                RpmPackage pkg;
                if (!pkg.parse(rpmFilePath)) {
                    db.unlock();
                    return 1;
                }

                RpmMetadata* oldPkg = db.find(pkg.meta.name);
                if (oldPkg && !opUpgrade) {
                    std::cerr << "package " << pkg.meta.name << " is already installed\n";
                    db.unlock();
                    return 1;
                }

                if (!optNoDeps && !DependencyEngine::verify(pkg.meta, db)) {
                    db.unlock();
                    return 1;
                }

                if (optTest) {
                    std::cout << "Test mode: package " << pkg.meta.name << " can be installed cleanly.\n";
                    continue;
                }

                PersistentWalJournal journal(targetRoot);

                if (!optNoScripts && !pkg.meta.preInScript.empty()) {
                    if (!ScriptletRunner::execute(pkg.meta.preInScript, "prein")) {
                        std::cerr << "error: %prein scriptlet failed.\n";
                        journal.rollback();
                        db.unlock();
                        return 1;
                    }
                }

                std::vector<FileRecord> extractedFiles;
                if (!ArchiveExtractionEngine::extract(rpmFilePath, pkg.payloadOffset, targetRoot, journal, extractedFiles, pkg.meta.files)) {
                    std::cerr << "error: archive extraction failed for " << rpmFilePath << "\n";
                    journal.rollback();
                    db.unlock();
                    return 1;
                }
                pkg.meta.files = extractedFiles;

                if (!optNoScripts && !pkg.meta.postInScript.empty()) {
                    if (!ScriptletRunner::execute(pkg.meta.postInScript, "postin")) {
                        std::cerr << "error: %postin scriptlet failed. Rolling back transaction.\n";
                        journal.rollback();
                        db.unlock();
                        return 1;
                    }
                }

                // If upgrading, run old package %preun, clean up obsolete files, and run %postun
                if (oldPkg) {
                    if (!optNoScripts && !oldPkg->preUnScript.empty()) {
                        ScriptletRunner::execute(oldPkg->preUnScript, "preun");
                    }
                    std::set<std::string> newPaths;
                    for (const auto& f : pkg.meta.files) newPaths.insert(f.path);
                    for (const auto& f : oldPkg->files) {
                        if (newPaths.count(f.path) == 0) {
                            std::error_code ec;
                            fs::remove(f.path, ec);
                        }
                    }
                    if (!optNoScripts && !oldPkg->postUnScript.empty()) {
                        ScriptletRunner::execute(oldPkg->postUnScript, "postun");
                    }
                }

                journal.commit();
                db.add(pkg.meta);

                if (optHash) {
                    printProgress(pkg.meta.name);
                } else if (optVerbose) {
                    std::cout << (oldPkg ? "Upgraded: " : "Installed: ")
                              << pkg.meta.name << "-" << pkg.meta.version << "-" << pkg.meta.release << "\n";
                }
            }
        } else if (opErase) {
            if (arguments.empty()) {
                std::cerr << "error: no packages given for erase\n";
                db.unlock();
                return 1;
            }

            for (const auto& pkgName : arguments) {
                RpmMetadata* found = db.find(pkgName);
                if (!found) {
                    std::cerr << "error: package " << pkgName << " is not installed\n";
                    db.unlock();
                    return 1;
                }

                if (!optNoDeps && !DependencyEngine::checkEraseDependencies(pkgName, db)) {
                    db.unlock();
                    return 1;
                }

                if (!optNoScripts && !found->preUnScript.empty()) {
                    ScriptletRunner::execute(found->preUnScript, "preun");
                }

                for (const auto& fileRec : found->files) {
                    std::error_code ec;
                    fs::remove(fileRec.path, ec);
                }

                if (!optNoScripts && !found->postUnScript.empty()) {
                    ScriptletRunner::execute(found->postUnScript, "postun");
                }

                db.erase(pkgName);
                if (optVerbose) std::cout << "Erased: " << pkgName << "\n";
            }
        }
        db.unlock();
        return 0;
    }

    // ========================================================================
    // VERIFY OPERATIONS
    // ========================================================================
    if (opVerify) {
        db.load();
        int verifyExitCode = 0;
        std::vector<std::string> targets = arguments;

        if (optAll) {
            targets.clear();
            for (const auto& [name, pkg] : db.all()) {
                targets.push_back(name);
            }
        }

        if (targets.empty()) {
            std::cerr << "error: no packages given for verify\n";
            return 1;
        }

        for (const auto& pkgName : targets) {
            RpmMetadata* found = db.find(pkgName);
            if (!found) {
                std::cerr << "package " << pkgName << " is not installed\n";
                verifyExitCode = 1;
                continue;
            }

            for (const auto& fileRec : found->files) {
                std::error_code ec;
                if (!fs::exists(fileRec.path, ec)) {
                    std::cout << "missing     " << fileRec.path << "\n";
                    verifyExitCode = 1;
                } else if (!fileRec.sha256.empty()) {
                    std::string currentHash = CngCryptoEngine::calculateFileSha256(fileRec.path);
                    if (currentHash != fileRec.sha256) {
                        std::cout << "..5......   " << fileRec.path << " (digest mismatch)\n";
                        verifyExitCode = 1;
                    }
                }
            }
        }
        return verifyExitCode;
    }

    return 0;
}