#include "iso_extractor.hpp"
#include "iso_format.hpp"
#include "page_aligned_buffer.hpp"
#include "win32_handle.hpp"
#include "xorriso_config.hpp"

bool IsoExtractor::ExtractDirectory(HANDLE hIso, UINT32 lba, UINT32 length, const fs::path& targetDir) {
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

bool IsoExtractor::Extract(const XorrisoConfig& cfg) {
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
