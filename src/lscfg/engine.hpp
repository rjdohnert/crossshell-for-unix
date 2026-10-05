#pragma once

#include "lscfg.hpp"
#include <map>

class StringHelper {
public:
    static std::string WideToNarrow(const std::wstring& wstr);
    static std::string VariantToString(const VARIANT& vt);
    static std::string FormatBytes(unsigned long long bytes);
};

class WmiEngine {
private:
    IWbemServices* pSvc = nullptr;
    IWbemLocator* pLoc = nullptr;
    bool initialized = false;

public:
    WmiEngine();
    ~WmiEngine();
    bool IsInitialized() const;
    std::vector<std::map<std::string, std::string>> Query(const std::wstring& wmiClass, const std::vector<std::wstring>& properties);
};

class InventoryCollector {
public:
    static std::vector<HardwareDevice> Collect(WmiEngine& wmi);
};
