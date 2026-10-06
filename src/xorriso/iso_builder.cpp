#include "file_entry.hpp"
#include "iso_builder.hpp"
#include "iso_codec.hpp"
#include "iso_format.hpp"
#include "page_aligned_buffer.hpp"
#include "win32_handle.hpp"
#include "xorriso_config.hpp"

bool IsoBuilder::Build(const XorrisoConfig& cfg) {
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
