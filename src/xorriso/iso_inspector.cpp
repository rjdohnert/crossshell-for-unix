#include "iso_format.hpp"
#include "iso_inspector.hpp"
#include "win32_handle.hpp"
#include "xorriso_config.hpp"

bool IsoInspector::Inspect(const XorrisoConfig& cfg) {
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
