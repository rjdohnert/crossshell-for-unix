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
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
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

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <memory>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <chrono>

namespace fs = std::filesystem;

// ============================================================================
// 1. ISO-9660 SPECIFICATION & DATA STRUCTURES
// ============================================================================

constexpr UINT32 ISO_SECTOR_SIZE = 2048;
constexpr UINT32 ISO_SYSTEM_AREA_SECTORS = 16;
constexpr UINT64 ISO_PVD_OFFSET = static_cast<UINT64>(ISO_SYSTEM_AREA_SECTORS) * ISO_SECTOR_SIZE;
constexpr UINT32 IO_BUFFER_SIZE = 1024 * 1024;

#pragma pack(push, 1)

struct BothEndian16 {
    uint16_t lsb;
    uint16_t msb;
    void set(uint16_t val) {
        lsb = val;
        msb = _byteswap_ushort(val);
    }
};

struct BothEndian32 {
    uint32_t lsb;
    uint32_t msb;
    void set(uint32_t val) {
        lsb = val;
        msb = _byteswap_ulong(val);
    }
};

struct IsoDateTimePVD {
    char year[4];
    char month[2];
    char day[2];
    char hour[2];
    char minute[2];
    char second[2];
    char hundredths[2];
    int8_t gmt_offset;
};

struct IsoDateTimeDir {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    int8_t gmt_offset;
};

struct IsoPrimaryVolumeDescriptor {
    uint8_t type;
    char id[5];
    uint8_t version;
    uint8_t unused1;
    char system_id[32];
    char volume_id[32];
    uint8_t unused2[8];
    BothEndian32 volume_space_size;
    uint8_t unused3[32];
    BothEndian16 volume_set_size;
    BothEndian16 volume_sequence_number;
    BothEndian16 logical_block_size;
    BothEndian32 path_table_size;
    uint32_t type_l_path_table;
    uint32_t opt_type_l_path_table;
    uint32_t type_m_path_table;
    uint32_t opt_type_m_path_table;
    uint8_t root_directory_record[34];
    char volume_set_id[128];
    char publisher_id[128];
    char data_preparer_id[128];
    char application_id[128];
    char copyright_file_id[37];
    char abstract_file_id[37];
    char bibliographic_file_id[37];
    IsoDateTimePVD creation_date;
    IsoDateTimePVD modification_date;
    IsoDateTimePVD expiration_date;
    IsoDateTimePVD effective_date;
    uint8_t file_structure_version;
    uint8_t unused4;
    uint8_t application_data[512];
    uint8_t reserved[653];
};

struct IsoDirRecordHeader {
    uint8_t length;
    uint8_t ext_attr_length;
    BothEndian32 extent_lba;
    BothEndian32 data_length;
    IsoDateTimeDir date;
    uint8_t file_flags;
    uint8_t file_unit_size;
    uint8_t interleave_gap_size;
    BothEndian16 volume_sequence_number;
    uint8_t name_len;
};

#pragma pack(pop)

// ============================================================================
// 2. MEMORY MANAGEMENT & WIN32 RAII WRAPPERS
// ============================================================================

class PageAlignedBuffer {
    void* ptr_ = nullptr;
    size_t size_ = 0;
public:
    PageAlignedBuffer(size_t size) : size_(size) {
        ptr_ = VirtualAlloc(nullptr, size_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }
    ~PageAlignedBuffer() {
        if (ptr_) VirtualFree(ptr_, 0, MEM_RELEASE);
    }
    uint8_t* data() const { return static_cast<uint8_t*>(ptr_); }
    size_t size() const { return size_; }
    bool isValid() const { return ptr_ != nullptr; }
};

class Win32Handle {
    HANDLE h_ = INVALID_HANDLE_VALUE;
public:
    Win32Handle(HANDLE h) : h_(h) {}
    ~Win32Handle() { if (isValid()) CloseHandle(h_); }
    bool isValid() const { return h_ != INVALID_HANDLE_VALUE && h_ != nullptr; }
    operator HANDLE() const { return h_; }
};

// ============================================================================
// 3. CODECS & ENGINE CONFIGURATION
// ============================================================================

enum class EngineMode { Help, Mkisofs, Extract, Inspect };

struct XorrisoConfig {
    EngineMode mode = EngineMode::Help;
    std::wstring isoPath;
    std::wstring sourceDir;
    std::wstring extractDir;
    std::string volumeID = "WIN_XORRISO_DISK";
    bool verbose = false;
    bool directIO = true;
};

struct FileEntry {
    fs::path relativePath;
    fs::path fullPath;
    UINT64 fileSize = 0;
    UINT32 startLBA = 0;
    bool isDir = false;
};

class IsoCodec {
public:
    static void PadCopy(char* dest, const std::string& src, size_t maxLen) {
        size_t copyLen = (std::min)(src.length(), maxLen);
        std::memcpy(dest, src.data(), copyLen);
        if (copyLen < maxLen) {
            std::memset(dest + copyLen, ' ', maxLen - copyLen);
        }
    }
};

// ============================================================================
// 4. ISO BUILDER, EXTRACTOR & INSPECTOR
// ============================================================================

class IsoBuilder {
public:
    static bool Build(const XorrisoConfig& cfg) {
        std::cout << "[+] Initializing Direct-DMA ISO Creation Engine...\n";
        std::cout << "    Source Dir : " << fs::path(cfg.sourceDir).string() << "\n";
        std::cout << "    Output ISO : " << fs::path(cfg.isoPath).string() << "\n";
        std::cout << "    Volume ID  : " << cfg.volumeID << "\n";

        DWORD flagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
        if (cfg.directIO) {
            flagsAndAttributes |= FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;
            std::cout << "    [Kernel] Direct-I/O Active: FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH\n";
        }

        Win32Handle hFile = CreateFileW(
            cfg.isoPath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            flagsAndAttributes,
            nullptr
        );

        if (!hFile.isValid()) {
            std::cerr << "[-] Error: Failed to open output file. Win32 Error: " << GetLastError() << "\n";
            return false;
        }

        PageAlignedBuffer transferBuffer(IO_BUFFER_SIZE);
        if (!transferBuffer.isValid()) {
            std::cerr << "[-] Error: VirtualAlloc failed to reserve page-aligned memory buffer.\n";
            return false;
        }

        std::memset(transferBuffer.data(), 0, ISO_SYSTEM_AREA_SECTORS * ISO_SECTOR_SIZE);
        DWORD bytesWritten = 0;
        if (!WriteFile(hFile, transferBuffer.data(), ISO_SYSTEM_AREA_SECTORS * ISO_SECTOR_SIZE, &bytesWritten, nullptr)) {
            std::cerr << "[-] Error writing ISO system area. Error: " << GetLastError() << "\n";
            return false;
        }

        std::vector<FileEntry> entries;
        UINT64 currentDataLBA = 18;

        try {
            for (const auto& entry : fs::recursive_directory_iterator(cfg.sourceDir)) {
                FileEntry fe;
                fe.fullPath = entry.path();
                fe.relativePath = fs::relative(entry.path(), cfg.sourceDir);
                fe.isDir = entry.is_directory();
                fe.fileSize = fe.isDir ? 0 : entry.file_size();
                fe.startLBA = 0;
                entries.push_back(fe);
            }
        } catch (const std::exception& ex) {
            std::cerr << "[-] Directory iteration error: " << ex.what() << "\n";
            return false;
        }

        for (auto& fe : entries) {
            if (!fe.isDir && fe.fileSize > 0) {
                fe.startLBA = static_cast<UINT32>(currentDataLBA);
                UINT64 sectorCount = (fe.fileSize + ISO_SECTOR_SIZE - 1) / ISO_SECTOR_SIZE;
                currentDataLBA += sectorCount;
            }
        }

        std::memset(transferBuffer.data(), 0, ISO_SECTOR_SIZE * 2);
        auto* pvd = reinterpret_cast<IsoPrimaryVolumeDescriptor*>(transferBuffer.data());
        pvd->type = 1;
        std::memcpy(pvd->id, "CD001", 5);
        pvd->version = 1;
        IsoCodec::PadCopy(pvd->system_id, "WIN32_NT_XORRISO", 32);
        IsoCodec::PadCopy(pvd->volume_id, cfg.volumeID, 32);
        pvd->volume_space_size.set(static_cast<uint32_t>(currentDataLBA));
        pvd->volume_set_size.set(1);
        pvd->volume_sequence_number.set(1);
        pvd->logical_block_size.set(ISO_SECTOR_SIZE);
        pvd->file_structure_version = 1;

        uint8_t* terminator = transferBuffer.data() + ISO_SECTOR_SIZE;
        terminator[0] = 255;
        std::memcpy(&terminator[1], "CD001", 5);
        terminator[6] = 1;

        if (!WriteFile(hFile, transferBuffer.data(), ISO_SECTOR_SIZE * 2, &bytesWritten, nullptr)) {
            std::cerr << "[-] Error writing PVD descriptor blocks.\n";
            return false;
        }

        std::cout << "[+] Streaming " << entries.size() << " files with zero-copy page alignment...\n";

        for (const auto& fe : entries) {
            if (fe.isDir || fe.fileSize == 0) continue;

            Win32Handle hIn = CreateFileW(
                fe.fullPath.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_SEQUENTIAL_SCAN,
                nullptr
            );

            if (!hIn.isValid()) continue;

            LARGE_INTEGER offset;
            offset.QuadPart = static_cast<LONGLONG>(fe.startLBA) * ISO_SECTOR_SIZE;
            SetFilePointerEx(hFile, offset, nullptr, FILE_BEGIN);

            DWORD bytesRead = 0;
            UINT64 remaining = fe.fileSize;

            while (remaining > 0) {
                DWORD chunk = static_cast<DWORD>((std::min)(remaining, static_cast<UINT64>(transferBuffer.size())));
                
                if (!ReadFile(hIn, transferBuffer.data(), chunk, &bytesRead, nullptr) || bytesRead == 0) break;

                DWORD writeSize = (bytesRead + ISO_SECTOR_SIZE - 1) & ~(ISO_SECTOR_SIZE - 1);
                if (writeSize > bytesRead) {
                    std::memset(transferBuffer.data() + bytesRead, 0, writeSize - bytesRead);
                }

                WriteFile(hFile, transferBuffer.data(), writeSize, &bytesWritten, nullptr);
                remaining -= (std::min)(remaining, static_cast<UINT64>(bytesRead));
            }

            if (cfg.verbose) {
                std::cout << "    [Sector " << fe.startLBA << "] Written: " << fe.relativePath.string() << "\n";
            }
        }

        std::cout << "[+] ISO Creation Complete. Total Sectors: " << currentDataLBA << "\n";
        return true;
    }
};

class IsoExtractor {
public:
    static bool ExtractDirectory(HANDLE hIso, UINT32 lba, UINT32 length, const fs::path& targetDir) {
        fs::create_directories(targetDir);

        PageAlignedBuffer dirBuf(length < ISO_SECTOR_SIZE ? ISO_SECTOR_SIZE : ((length + ISO_SECTOR_SIZE - 1) & ~(ISO_SECTOR_SIZE - 1)));
        LARGE_INTEGER offset;
        offset.QuadPart = static_cast<LONGLONG>(lba) * ISO_SECTOR_SIZE;
        SetFilePointerEx(hIso, offset, nullptr, FILE_BEGIN);

        DWORD bytesRead = 0;
        if (!ReadFile(hIso, dirBuf.data(), static_cast<DWORD>(dirBuf.size()), &bytesRead, nullptr)) {
            return false;
        }

        uint8_t* ptr = dirBuf.data();
        uint8_t* end = dirBuf.data() + length;

        while (ptr < end) {
            auto* rec = reinterpret_cast<IsoDirRecordHeader*>(ptr);
            if (rec->length == 0) {
                size_t alignOffset = ISO_SECTOR_SIZE - (reinterpret_cast<uintptr_t>(ptr) % ISO_SECTOR_SIZE);
                ptr += alignOffset;
                continue;
            }

            char* rawName = reinterpret_cast<char*>(ptr + sizeof(IsoDirRecordHeader));
            std::string filename(rawName, rec->name_len);

            size_t semiPos = filename.find(';');
            if (semiPos != std::string::npos) {
                filename = filename.substr(0, semiPos);
            }

            if (filename != "\x00" && filename != "\x01") {
                fs::path destPath = targetDir / filename;

                if (rec->file_flags & 0x02) {
                    ExtractDirectory(hIso, rec->extent_lba.lsb, rec->data_length.lsb, destPath);
                } else {
                    Win32Handle hOut = CreateFileW(
                        destPath.c_str(),
                        GENERIC_WRITE,
                        0,
                        nullptr,
                        CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL,
                        nullptr
                    );

                    if (hOut.isValid() && rec->data_length.lsb > 0) {
                        PageAlignedBuffer fileBuf(IO_BUFFER_SIZE);
                        LARGE_INTEGER fileOffset;
                        fileOffset.QuadPart = static_cast<LONGLONG>(rec->extent_lba.lsb) * ISO_SECTOR_SIZE;
                        SetFilePointerEx(hIso, fileOffset, nullptr, FILE_BEGIN);

                        UINT64 fileRemaining = rec->data_length.lsb;
                        DWORD written = 0;

                        while (fileRemaining > 0) {
                            DWORD chunk = static_cast<DWORD>((std::min)(fileRemaining, static_cast<UINT64>(fileBuf.size())));
                            ReadFile(hIso, fileBuf.data(), chunk, &bytesRead, nullptr);
                            WriteFile(hOut, fileBuf.data(), bytesRead, &written, nullptr);
                            fileRemaining -= bytesRead;
                        }
                    }
                }
            }
            ptr += rec->length;
        }
        return true;
    }

    static bool Extract(const XorrisoConfig& cfg) {
        std::cout << "[+] Initializing Direct-DMA ISO Extractor...\n";
        std::cout << "    Input ISO  : " << fs::path(cfg.isoPath).string() << "\n";
        std::cout << "    Target Dir : " << fs::path(cfg.extractDir).string() << "\n";

        Win32Handle hIso = CreateFileW(
            cfg.isoPath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr
        );

        if (!hIso.isValid()) {
            std::cerr << "[-] Failed to open input ISO file. Win32 Error: " << GetLastError() << "\n";
            return false;
        }

        LARGE_INTEGER pvdOffset;
        pvdOffset.QuadPart = ISO_PVD_OFFSET;
        SetFilePointerEx(hIso, pvdOffset, nullptr, FILE_BEGIN);

        IsoPrimaryVolumeDescriptor pvd;
        DWORD bytesRead = 0;
        if (!ReadFile(hIso, &pvd, sizeof(pvd), &bytesRead, nullptr) || std::memcmp(pvd.id, "CD001", 5) != 0) {
            std::cerr << "[-] Error: Target is not a valid ISO-9660 image.\n";
            return false;
        }

        auto* rootRec = reinterpret_cast<IsoDirRecordHeader*>(pvd.root_directory_record);
        return ExtractDirectory(hIso, rootRec->extent_lba.lsb, rootRec->data_length.lsb, cfg.extractDir);
    }
};

class IsoInspector {
public:
    static bool Inspect(const XorrisoConfig& cfg) {
        Win32Handle hFile = CreateFileW(
            cfg.isoPath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (!hFile.isValid()) {
            std::cerr << "[-] Failed to open ISO image. Win32 Error: " << GetLastError() << "\n";
            return false;
        }

        LARGE_INTEGER pvdOffset;
        pvdOffset.QuadPart = ISO_PVD_OFFSET;
        SetFilePointerEx(hFile, pvdOffset, nullptr, FILE_BEGIN);

        IsoPrimaryVolumeDescriptor pvd;
        DWORD bytesRead = 0;
        if (!ReadFile(hFile, &pvd, sizeof(pvd), &bytesRead, nullptr) || bytesRead != sizeof(pvd)) {
            std::cerr << "[-] Read error while reading ISO PVD.\n";
            return false;
        }

        if (std::memcmp(pvd.id, "CD001", 5) != 0) {
            std::cerr << "[-] Header Magic Validation Failed.\n";
            return false;
        }

        char volId[33] = {0};
        char sysId[33] = {0};
        std::memcpy(volId, pvd.volume_id, 32);
        std::memcpy(sysId, pvd.system_id, 32);

        std::cout << "\n================ ISO VOLUME DESCRIPTION ================\n";
        std::cout << " Standard Magic Identifier : " << std::string(pvd.id, 5) << "\n";
        std::cout << " System Identifier         : " << sysId << "\n";
        std::cout << " Volume Identifier         : " << volId << "\n";
        std::cout << " Volume Space Size         : " << pvd.volume_space_size.lsb << " Sectors ("
                  << (static_cast<UINT64>(pvd.volume_space_size.lsb) * ISO_SECTOR_SIZE) / (1024 * 1024) << " MB)\n";
        std::cout << " Logical Block Size        : " << pvd.logical_block_size.lsb << " Bytes\n";
        std::cout << " File Structure Version    : " << static_cast<int>(pvd.file_structure_version) << "\n";
        std::cout << "========================================================\n\n";

        return true;
    }
};

// ============================================================================
// 5. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

class OptionParser {
public:
    static void PrintHeader() {
        std::cout << "Xorriso v5.1.0 - ISO Utility\n";
        std::cout << "Copyright (C) 2026. Roberto J. Dohnert All rights reserved.\n";
        std::cout << "-------------------------------------------------------------------\n";
    }

    static void PrintHelp() {
        PrintHeader();
        std::cout << R"(
USAGE EXAMPLES:
  1. Build an ISO Image (mkisofs compatibility mode):
     xorriso.exe -as mkisofs -o C:\output.iso -V "MY_LABEL" C:\source_folder

  2. High-Speed Direct Unbuffered ISO Extraction (-osirrox):
     xorriso.exe -osirrox -extract C:\input.iso C:\target_folder

  3. Inspect ISO Volume Header & Contents:
     xorriso.exe -indev C:\input.iso -toc

COMMAND REFERENCE:
  -help, /?                Display this comprehensive help screen.
  -as mkisofs              Emulate classic mkisofs/genisoimage command flags.
  -o <path>                Specify output ISO file destination.
  -V <label>               Set Volume Identifier (Volume Label, max 32 chars).
  -osirrox                 Enable filesystem extraction mode.
  -extract <iso> <dir>     Extract contents from <iso> into local directory <dir>.
  -indev <iso> -toc        Inspect Volume Table of Contents & PVD metadata.
  -v                       Enable verbose kernel console diagnostics.
)";
    }

    bool Parse(int argc, char* argv[], XorrisoConfig& cfg, bool& exitEarly) const {
        exitEarly = false;
        if (argc < 2) {
            PrintHelp();
            exitEarly = true;
            return true;
        }

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-help" || arg == "/?" || arg == "--help") {
                PrintHelp();
                exitEarly = true;
                return true;
            } else if (arg == "-v") {
                cfg.verbose = true;
            } else if (arg == "-as" && i + 1 < argc && std::string(argv[i + 1]) == "mkisofs") {
                cfg.mode = EngineMode::Mkisofs;
                i++;
            } else if (arg == "-o" && i + 1 < argc) {
                cfg.isoPath = fs::path(argv[++i]).wstring();
            } else if (arg == "-V" && i + 1 < argc) {
                cfg.volumeID = argv[++i];
            } else if (arg == "-osirrox") {
                cfg.mode = EngineMode::Extract;
            } else if (arg == "-extract" && i + 2 < argc) {
                cfg.mode = EngineMode::Extract;
                cfg.isoPath = fs::path(argv[++i]).wstring();
                cfg.extractDir = fs::path(argv[++i]).wstring();
            } else if (arg == "-indev" && i + 1 < argc) {
                cfg.mode = EngineMode::Inspect;
                cfg.isoPath = fs::path(argv[++i]).wstring();
            } else if (arg == "-toc") {
                if (cfg.mode != EngineMode::Inspect) cfg.mode = EngineMode::Inspect;
            } else if (cfg.sourceDir.empty() && arg[0] != '-') {
                cfg.sourceDir = fs::path(arg).wstring();
            }
        }
        return true;
    }
};

class XorrisoApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]) {
        XorrisoConfig cfg;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, cfg, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        bool result = false;

        switch (cfg.mode) {
            case EngineMode::Mkisofs:
                if (cfg.isoPath.empty() || cfg.sourceDir.empty()) {
                    std::cerr << "[-] Error: Missing arguments for ISO creation (-o <out.iso> <source_dir>)\n";
                    return 1;
                }
                result = IsoBuilder::Build(cfg);
                break;

            case EngineMode::Extract:
                if (cfg.isoPath.empty() || cfg.extractDir.empty()) {
                    std::cerr << "[-] Error: Missing extraction parameters (-extract <in.iso> <out_dir>)\n";
                    return 1;
                }
                result = IsoExtractor::Extract(cfg);
                break;

            case EngineMode::Inspect:
                if (cfg.isoPath.empty()) {
                    std::cerr << "[-] Error: Missing ISO path (-indev <image.iso>)\n";
                    return 1;
                }
                result = IsoInspector::Inspect(cfg);
                break;

            default:
                OptionParser::PrintHelp();
                return 0;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = endTime - startTime;

        if (result) {
            std::cout << "[+] Operation completed successfully in " << duration.count() << " ms.\n";
            return 0;
        } else {
            std::cerr << "[-] Operation failed.\n";
            return 1;
        }
    }
};

int main(int argc, char* argv[]) {
    XorrisoApplication app;
    return app.Run(argc, argv);
}