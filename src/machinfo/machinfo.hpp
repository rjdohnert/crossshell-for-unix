#ifndef MACHINFO_HPP
#define MACHINFO_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>

#pragma pack(push, 1)
struct RawSMBIOSData {
    BYTE Used20CallingMethod;
    BYTE MajorVersion;
    BYTE MinorVersion;
    BYTE DmiRevision;
    DWORD Length;
    BYTE SMBIOSTableData[1];
};

struct SMBIOSHeader {
    BYTE Type;
    BYTE Length;
    WORD Handle;
};
#pragma pack(pop)

struct CpuTopology {
    std::wstring name = L"Unknown Processor";
    DWORD clockMhz{0};
    DWORD socketCount{0};
    DWORD coreCount{0};
    DWORD logicalCount{0};
    DWORD numaNodeCount{0};
    ULONGLONG l1CacheBytes{0};
    ULONGLONG l2CacheBytes{0};
    ULONGLONG l3CacheBytes{0};
    bool is64Bit{true};
    bool supportsVirtualization{false};
    bool supportsAVX2{false};
};

struct SystemFirmwareInfo {
    std::wstring manufacturer = L"N/A";
    std::wstring productName = L"N/A";
    std::wstring serialNumber = L"N/A";
    std::wstring uuid = L"N/A";
    std::wstring biosVendor = L"N/A";
    std::wstring biosVersion = L"N/A";
    std::wstring biosReleaseDate = L"N/A";
    std::wstring firmwareType = L"Unknown";
};

struct MemoryStatusInfo {
    ULONGLONG totalPhysBytes{0};
    ULONGLONG availPhysBytes{0};
    ULONGLONG pageFileLimitBytes{0};
    double loadPercentage{0.0};
};

struct TpmInfo {
    bool present{false};
    bool ready{false};
    std::wstring specVersion = L"N/A";
    std::wstring manufacturer = L"N/A";
    std::wstring manufacturerVersion = L"N/A";
};

#endif // MACHINFO_HPP
