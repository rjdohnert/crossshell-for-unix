// ============================================================================
// 4. Handle Collector Service
// ============================================================================
class HandleCollector {
public:
    static std::vector<ProcessHandleInfo> collect() {
        std::vector<ProcessHandleInfo> results;

        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) return results;

        auto querySystem = reinterpret_cast<NtQuerySystemInformationFn>(
            GetProcAddress(ntdll, "NtQuerySystemInformation"));
        if (!querySystem) return results;

        ULONG size = 256 * 1024;
        std::vector<BYTE> buffer(size);
        ULONG returned = 0;
        while (querySystem(static_cast<SYSTEM_INFORMATION_CLASS>(5), buffer.data(), size, &returned) != 0) {
            size = (returned > size) ? (returned + 32768) : (size * 2);
            if (size > 64 * 1024 * 1024) return results;
            buffer.resize(size);
        }

        BYTE* p = buffer.data();
        while (p) {
            auto* spi = reinterpret_cast<SYSTEM_PROCESS_INFORMATION_ENTRY*>(p);
            ProcessHandleInfo info;
            info.pid = static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(spi->UniqueProcessId));
            info.handleCount = spi->HandleCount;
            info.accessDenied = false;

            if (spi->ImageName.Buffer && spi->ImageName.Length > 0) {
                info.name.assign(spi->ImageName.Buffer, spi->ImageName.Length / sizeof(wchar_t));
            } else {
                if (info.pid == 0) info.name = L"System Idle Process";
                else if (info.pid == 4) info.name = L"System";
                else info.name = L"<unknown>";
            }

            results.push_back(info);

            if (spi->NextEntryOffset == 0) break;
            p += spi->NextEntryOffset;
        }

        return results;
    }
};


