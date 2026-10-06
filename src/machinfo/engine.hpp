#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "machinfo.hpp"
#include "options.hpp"

class SmbiosParser {
public:
    static std::wstring getSmbiosString(const BYTE* headerPtr, BYTE stringIndex, const BYTE* endPtr);
    static SystemFirmwareInfo parseFirmware();
};

class RegistryHelper {
public:
    static bool readString(HKEY hRoot, const wchar_t* subkeyPath, const wchar_t* valueName, std::wstring& outValue);
    static bool readDword(HKEY hRoot, const wchar_t* subkeyPath, const wchar_t* valueName, DWORD& outValue);
};

class TpmDiagnosticsProvider {
private:
    static std::wstring manufacturerIdToString(DWORD id);
public:
    static TpmInfo query(bool enableTpmWmi);
};

class HardwareDiagnosticsScanner {
private:
    static bool checkAVX2Support();
public:
    static CpuTopology queryCpuTopology();
    static MemoryStatusInfo queryMemoryStatus();
    static std::wstring queryWindowsVersion();
};

class MachinfoEngine {
private:
    MachinfoOptions options;
public:
    explicit MachinfoEngine(MachinfoOptions opts);
    int execute();
};

#endif // ENGINE_HPP
