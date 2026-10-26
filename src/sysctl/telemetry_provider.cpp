#include "telemetry_provider.hpp"
#include "sysctl.hpp"
#include <chrono>

using namespace sysctl;

MEMORYSTATUSEX WindowsTelemetryFactory::GetMemoryStatus() {
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status)) {
        ThrowWindowsError(GetLastError(), "GlobalMemoryStatusEx");
    }
    return status;
}

PageFileUsage WindowsTelemetryFactory::GetPageFileUsage() {
    PageFileUsage usage{};
    EnumPageFilesW([](LPVOID context, PENUM_PAGE_FILE_INFORMATION info, LPCWSTR) -> BOOL {
        if (context && info) {
            auto* u = reinterpret_cast<PageFileUsage*>(context);
            u->totalPages += info->TotalSize;
            u->usedPages += info->TotalInUse;
        }
        return TRUE;
    }, &usage);
    return usage;
}

DWORD WindowsTelemetryFactory::GetPageSize() {
    SYSTEM_INFO info{};
    GetNativeSystemInfo(&info);
    return info.dwPageSize;
}

DWORD WindowsTelemetryFactory::GetCpuCount() {
    const DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (count == 0) ThrowWindowsError(GetLastError(), "GetActiveProcessorCount");
    return count;
}

std::string WindowsTelemetryFactory::GetOsRelease() {
    OSVERSIONINFOW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    using RtlGetVersionPtr = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);
    HMODULE module = GetModuleHandleW(L"ntdll.dll");
    if (!module) {
        throw std::system_error(GetLastError(), std::system_category(), "GetModuleHandleW");
    }
    auto getVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(module, "RtlGetVersion"));
    if (!getVersion) {
        throw std::system_error(GetLastError(), std::system_category(), "RtlGetVersion lookup");
    }
    if (getVersion(&osvi) != 0) {
        throw std::runtime_error("RtlGetVersion failed");
    }
    return std::to_string(osvi.dwMajorVersion) + "." +
           std::to_string(osvi.dwMinorVersion) + "." +
           std::to_string(osvi.dwBuildNumber);
}

void WindowsTelemetryFactory::Populate(MibTree& tree) {
    // --- kern.* (Kernel Telemetry) ---
    tree.RegisterNode("kern.ostype", "Operating system type", []() {
        return "Windows_NT";
    });

    tree.RegisterNode("kern.osrelease", "Operating system release version", []() {
        return GetOsRelease();
    });

    tree.RegisterNode("kern.version", "Kernel version information", []() {
        return "Microsoft Windows [Version " + GetOsRelease() + "] NT Kernel Engine";
    });

    tree.RegisterNode("kern.hostname", "System hostname",
        []() -> std::string {
            wchar_t buf[256]{};
            DWORD len = ARRAYSIZE(buf);
            if (!GetComputerNameExW(ComputerNameDnsHostname, buf, &len)) {
                ThrowWindowsError(GetLastError(), "GetComputerNameExW");
            }
            return WideToUtf8(std::wstring(buf, len));
        },
        [](const std::string& newName) -> bool {
            if (newName.empty() || newName.find('\0') != std::string::npos) {
                throw std::invalid_argument("hostname must be nonempty and contain no NUL characters");
            }
            const std::wstring ws = Utf8ToWide(newName);
            if (!SetComputerNameExW(ComputerNamePhysicalDnsHostname, ws.c_str())) {
                ThrowWindowsError(GetLastError(), "SetComputerNameExW (administrator privileges required)");
            }
            return true;
        }
    );

    tree.RegisterNode("kern.domainname", "System DNS domain name", []() -> std::string {
        wchar_t buf[256]{};
        DWORD len = ARRAYSIZE(buf);
        if (!GetComputerNameExW(ComputerNameDnsDomain, buf, &len)) {
            ThrowWindowsError(GetLastError(), "GetComputerNameExW(ComputerNameDnsDomain)");
        }
        return WideToUtf8(std::wstring(buf, len));
    });

    tree.RegisterNode("kern.boottime", "Estimated system boot time (date, or epoch seconds with -n)", []() {
        auto now = std::chrono::system_clock::now();
        auto uptimeMs = GetTickCount64();
        auto bootTime = now - std::chrono::milliseconds(uptimeMs);
        auto epochSec = std::chrono::duration_cast<std::chrono::seconds>(bootTime.time_since_epoch()).count();
        return std::to_string(epochSec);
    });

    // --- hw.* (Hardware Telemetry) ---
    tree.RegisterNode("hw.machine", "Target machine architecture", []() {
        SYSTEM_INFO si;
        GetNativeSystemInfo(&si);
        switch (si.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64: return "amd64";
            case PROCESSOR_ARCHITECTURE_ARM64: return "arm64";
            case PROCESSOR_ARCHITECTURE_INTEL: return "i386";
            case PROCESSOR_ARCHITECTURE_ARM:   return "arm";
            default:                           return "unknown";
        }
    });

    tree.RegisterNode("hw.ncpu", "Number of active logical CPUs", []() {
        return std::to_string(GetCpuCount());
    });

    tree.RegisterNode("hw.ncpuonline", "Number of online CPUs", []() {
        return std::to_string(GetCpuCount());
    });

    tree.RegisterNode("hw.pagesize", "Hardware page size in bytes", []() {
        return std::to_string(GetPageSize());
    });

    tree.RegisterNode("hw.byteorder", "Hardware byte ordering", []() {
        return "1234"; // Little-Endian (OpenBSD notation)
    });

    tree.RegisterNode("hw.physmem", "Total physical RAM in bytes", []() {
        return std::to_string(GetMemoryStatus().ullTotalPhys);
    });

    tree.RegisterNode("hw.physmem64", "Total physical RAM in bytes (64-bit)", []() {
        return std::to_string(GetMemoryStatus().ullTotalPhys);
    });

    tree.RegisterNode("hw.usermem", "OpenBSD non-kernel memory (not available on Windows)", []() {
        return "value is not available on Windows; hw.freemem reports available RAM, not non-kernel memory";
    }, nullptr, SysctlLeafNode::Kind::Unavailable);

    tree.RegisterNode("hw.freemem", "Windows available physical RAM in bytes", []() {
        return std::to_string(GetMemoryStatus().ullAvailPhys);
    });

    tree.RegisterNode("hw.model", "CPU Brand identification string", []() {
        wchar_t buf[256]{};
        DWORD bytes = sizeof(buf);
        const LSTATUS status = RegGetValueW(HKEY_LOCAL_MACHINE,
            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString",
            RRF_RT_REG_SZ, nullptr, buf, &bytes);
        if (status != ERROR_SUCCESS) {
            ThrowWindowsError(status, "RegGetValueW(ProcessorNameString)");
        }
        return Trim(WideToUtf8(buf));
    });

    tree.RegisterNode("hw.cpuspeed", "CPU frequency in MHz", []() {
        DWORD mhz = 0;
        DWORD bytes = sizeof(mhz);
        const LSTATUS status = RegGetValueW(HKEY_LOCAL_MACHINE,
            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"~MHz",
            RRF_RT_REG_DWORD, nullptr, &mhz, &bytes);
        if (status != ERROR_SUCCESS) {
            ThrowWindowsError(status, "RegGetValueW(~MHz)");
        }
        return std::to_string(mhz);
    });

    // --- vm.* (Virtual Memory Subsystem) ---
    tree.RegisterNode("vm.loadavg", "Unix load averages (not available on Windows)", []() {
        return "value is not available on Windows; vm.memory_load is a memory percentage, not a load average";
    }, nullptr, SysctlLeafNode::Kind::Unavailable);

    tree.RegisterNode("vm.memory_load", "Windows physical memory load percentage (0-100)", []() {
        return std::to_string(GetMemoryStatus().dwMemoryLoad);
    });

    tree.RegisterNode("vm.commit_limit", "Windows system commit limit in bytes", []() {
        return std::to_string(GetMemoryStatus().ullTotalPageFile);
    });

    tree.RegisterNode("vm.commit_available", "Windows available commit for this process in bytes", []() {
        return std::to_string(GetMemoryStatus().ullAvailPageFile);
    });

    tree.RegisterNode("vm.swap_total", "Total pagefile size in bytes", []() {
        return std::to_string(GetPageFileUsage().totalPages * GetPageSize());
    });

    tree.RegisterNode("vm.swap_free", "Free pagefile space in bytes", []() {
        const auto usage = GetPageFileUsage();
        return std::to_string((usage.totalPages - usage.usedPages) * GetPageSize());
    });

    // --- net.inet.* (Network Stack via IP Helper) ---
    tree.RegisterNode("net.inet6.ip6.forwarding", "IPv6 forwarding state (0=Disabled, 1=Enabled)",
        []() -> std::string {
            MIB_IPSTATS stats{};
#ifndef AF_INET6
            constexpr ULONG afInet6 = 23;
#else
            constexpr ULONG afInet6 = AF_INET6;
#endif
            const DWORD status = GetIpStatisticsEx(&stats, afInet6);
            if (status != NO_ERROR) ThrowWindowsError(status, "GetIpStatisticsEx(AF_INET6)");
            return (stats.dwForwarding == MIB_IP_FORWARDING) ? "1" : "0";
        }
    );

    tree.RegisterNode("net.inet.ip.forwarding", "IP forwarding state (0=Disabled, 1=Enabled)",
        []() -> std::string {
            MIB_IPSTATS stats;
            const DWORD status = GetIpStatistics(&stats);
            if (status != NO_ERROR) ThrowWindowsError(status, "GetIpStatistics");
            return (stats.dwForwarding == MIB_IP_FORWARDING) ? "1" : "0";
        }
    );
}
