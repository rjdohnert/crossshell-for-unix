#include "engine.hpp"

std::string SystemConfigScanner::toUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &out[0], size, nullptr, nullptr);
    return out;
}

std::wstring SystemConfigScanner::getPropString(HDEVINFO info, SP_DEVINFO_DATA& dev, const DEVPROPKEY& key) {
    DEVPROPTYPE type = 0;
    DWORD needed = 0;
    SetupDiGetDevicePropertyW(info, &dev, &key, &type, nullptr, 0, &needed, 0);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || needed == 0) {
        return L"";
    }

    std::vector<BYTE> buf(needed);
    if (!SetupDiGetDevicePropertyW(info, &dev, &key, &type, buf.data(), needed, nullptr, 0)) {
        return L"";
    }

    if (type == DEVPROP_TYPE_STRING || type == DEVPROP_TYPE_STRING_LIST) {
        return std::wstring(reinterpret_cast<const wchar_t*>(buf.data()));
    }
    return L"";
}

std::string SystemConfigScanner::archWord(WORD arch) {
    switch (arch) {
        case PROCESSOR_ARCHITECTURE_AMD64: return "x86_64";
        case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
        case PROCESSOR_ARCHITECTURE_ARM64: return "arm64";
        case PROCESSOR_ARCHITECTURE_ARM:   return "arm";
        default: return "unknown";
    }
}

SystemConfigSummary SystemConfigScanner::collect() {
    SystemConfigSummary summary;

    wchar_t computerName[256]{};
    DWORD computerNameSize = static_cast<DWORD>(std::size(computerName));
    if (GetComputerNameExW(ComputerNamePhysicalDnsHostname, computerName, &computerNameSize)) {
        summary.nodeName = toUtf8(computerName);
    }

    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    summary.kernelArchitecture = archWord(si.wProcessorArchitecture);
    summary.numProcessors = si.dwNumberOfProcessors;

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    summary.totalMemoryMb = mem.ullTotalPhys / (1024ULL * 1024ULL);

    OSVERSIONINFOW osv{};
    osv.dwOSVersionInfoSize = sizeof(osv);
    GetVersionExW(&osv);
    summary.osLevel = std::to_string(osv.dwMajorVersion) + "." +
                      std::to_string(osv.dwMinorVersion) + " (Build " +
                      std::to_string(osv.dwBuildNumber) + ")";

    HDEVINFO info = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (info != INVALID_HANDLE_VALUE) {
        DWORD idx = 0;
        SP_DEVINFO_DATA dev{};
        dev.cbSize = sizeof(dev);

        while (SetupDiEnumDeviceInfo(info, idx++, &dev)) {
            std::wstring devClass = getPropString(info, dev, DEVPKEY_Device_Class);
            if (devClass.empty()) devClass = L"Unknown";
            ++summary.classCounts[toUtf8(devClass)];
            ++summary.totalDevices;
        }
        SetupDiDestroyDeviceInfoList(info);
    }

    return summary;
}

PrtconfEngine::PrtconfEngine(PrtconfOptions opts) : options(opts) {}

int PrtconfEngine::execute() {
    SetConsoleOutputCP(CP_UTF8);
    SystemConfigSummary summary = SystemConfigScanner::collect();
    PrtconfReporter::report(summary, options.showDevices);
    return 0;
}
