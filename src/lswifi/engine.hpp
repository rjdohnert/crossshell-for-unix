#pragma once

#include "lswifi.hpp"
#include <utility>

class WifiUtils {
public:
    static std::string GuidToString(const GUID& guid);
    static std::string MacToString(const unsigned char* mac);
    static std::string ExtractManufacturer(const std::string& desc);
    static std::string WideToUtf8(const std::wstring& wide);
    static std::pair<uint32_t, std::string> FreqToChannelAndBand(uint32_t freqKhz);
    static std::string AuthToString(DOT11_AUTH_ALGORITHM auth);
    static std::string CipherToString(DOT11_CIPHER_ALGORITHM cipher);
    static std::string EscapeJson(const std::string& s);
};

class WifiManager {
private:
    HANDLE m_hClient = nullptr;
    DWORD m_negotiatedVersion = 0;

    std::vector<AccessPoint> RetrieveAccessPoints(const GUID& guid, const std::string& currentBssid);

public:
    WifiManager();
    ~WifiManager();

    std::vector<WifiAdapter> EnumerateAdapters(bool triggerScan = false);
};
