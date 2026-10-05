#include "engine.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

std::string WifiUtils::GuidToString(const GUID& guid) {
    char buf[40];
    snprintf(buf, sizeof(buf), "{%08lX-%04hX-%04hX-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return std::string(buf);
}

std::string WifiUtils::MacToString(const unsigned char* mac) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(buf);
}

std::string WifiUtils::ExtractManufacturer(const std::string& desc) {
    static const std::vector<std::string> vendors = {
        "Intel", "Realtek", "Broadcom", "Qualcomm", "MediaTek",
        "Atheros", "TP-Link", "Killer", "Marvell", "Ralink", "ASUS"
    };
    for (const auto& v : vendors) {
        if (desc.find(v) != std::string::npos) return v;
    }
    return "Generic / Other";
}

std::string WifiUtils::WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};

    const int count = WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.c_str(),
        static_cast<int>(wide.length()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (count <= 0) return {};

    std::string result(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.c_str(),
        static_cast<int>(wide.length()),
        &result[0],
        count,
        nullptr,
        nullptr
    );
    return result;
}

std::pair<uint32_t, std::string> WifiUtils::FreqToChannelAndBand(uint32_t freqKhz) {
    uint32_t freqMhz = freqKhz / 1000;
    if (freqMhz == 2484) return { 14, "2.4 GHz" };
    if (freqMhz >= 2412 && freqMhz <= 2472) return { (freqMhz - 2407) / 5, "2.4 GHz" };
    if (freqMhz >= 5170 && freqMhz <= 5825) return { (freqMhz - 5000) / 5, "5 GHz" };
    if (freqMhz >= 5955 && freqMhz <= 7115) return { (freqMhz - 5950) / 5, "6 GHz" };
    return { 0, "Unknown" };
}

std::string WifiUtils::AuthToString(DOT11_AUTH_ALGORITHM auth) {
    switch (auth) {
        case DOT11_AUTH_ALGO_80211_OPEN: return "Open";
        case DOT11_AUTH_ALGO_80211_SHARED_KEY: return "WEP";
        case DOT11_AUTH_ALGO_WPA: return "WPA-Enterprise";
        case DOT11_AUTH_ALGO_WPA_PSK: return "WPA-PSK";
        case DOT11_AUTH_ALGO_RSNA: return "WPA2-Enterprise";
        case DOT11_AUTH_ALGO_RSNA_PSK: return "WPA2-PSK";
        case DOT11_AUTH_ALGO_WPA3_SAE: return "WPA3-SAE";
        case DOT11_AUTH_ALGO_WPA3_ENT: return "WPA3-Enterprise";
        default: return "Other";
    }
}

std::string WifiUtils::CipherToString(DOT11_CIPHER_ALGORITHM cipher) {
    switch (cipher) {
        case DOT11_CIPHER_ALGO_NONE: return "None";
        case DOT11_CIPHER_ALGO_WEP40:
        case DOT11_CIPHER_ALGO_WEP104: return "WEP";
        case DOT11_CIPHER_ALGO_TKIP: return "TKIP";
        case DOT11_CIPHER_ALGO_CCMP: return "AES-CCMP";
        case DOT11_CIPHER_ALGO_GCMP: return "GCMP";
        default: return "Other";
    }
}

std::string WifiUtils::EscapeJson(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\b') o << "\\b";
        else if (c == '\f') o << "\\f";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else if (static_cast<unsigned char>(c) <= 0x1f) {
            o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
        } else {
            o << c;
        }
    }
    return o.str();
}

WifiManager::WifiManager() {
    DWORD dwResult = WlanOpenHandle(2, nullptr, &m_negotiatedVersion, &m_hClient);
    if (dwResult != ERROR_SUCCESS) {
        throw std::runtime_error("Failed to open WLAN Handle. (Error Code: " + std::to_string(dwResult) + ")");
    }
}

WifiManager::~WifiManager() {
    if (m_hClient) {
        WlanCloseHandle(m_hClient, nullptr);
        m_hClient = nullptr;
    }
}

std::vector<AccessPoint> WifiManager::RetrieveAccessPoints(const GUID& guid, const std::string& currentBssid) {
    std::vector<AccessPoint> apList;
    PWLAN_BSS_LIST pBssList = nullptr;

    DWORD dwResult = WlanGetNetworkBssList(m_hClient, &guid, nullptr, dot11_BSS_type_any, FALSE, nullptr, &pBssList);
    if (dwResult != ERROR_SUCCESS || !pBssList) return apList;

    for (DWORD j = 0; j < pBssList->dwNumberOfItems; ++j) {
        const WLAN_BSS_ENTRY& bss = pBssList->wlanBssEntries[j];
        AccessPoint ap;

        ap.bssid = WifiUtils::MacToString(bss.dot11Bssid);
        if (bss.dot11Ssid.uSSIDLength > 0) {
            ap.ssid = std::string((char*)bss.dot11Ssid.ucSSID, bss.dot11Ssid.uSSIDLength);
        } else {
            ap.ssid = "[Hidden Network]";
        }

        ap.rssiDbm = bss.lRssi;
        ap.signalQuality = bss.uLinkQuality;
        ap.frequencyKhz = bss.ulChCenterFrequency;

        const auto channelAndBand = WifiUtils::FreqToChannelAndBand(bss.ulChCenterFrequency);
        ap.channel = channelAndBand.first;
        ap.band = channelAndBand.second;
        ap.isConnected = (!currentBssid.empty() && _stricmp(currentBssid.c_str(), ap.bssid.c_str()) == 0);

        // Fetch capabilities / cipher
        ap.authAlgorithm = "802.11";
        ap.cipherAlgorithm = "N/A";

        apList.push_back(ap);
    }

    if (pBssList) WlanFreeMemory(pBssList);
    return apList;
}

std::vector<WifiAdapter> WifiManager::EnumerateAdapters(bool triggerScan) {
    std::vector<WifiAdapter> adapters;
    PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;

    DWORD dwResult = WlanEnumInterfaces(m_hClient, nullptr, &pIfList);
    if (dwResult != ERROR_SUCCESS || !pIfList) return adapters;

    for (DWORD i = 0; i < pIfList->dwNumberOfItems; ++i) {
        PWLAN_INTERFACE_INFO pIfInfo = &pIfList->InterfaceInfo[i];
        WifiAdapter adapter;
        adapter.guid = WifiUtils::GuidToString(pIfInfo->InterfaceGuid);

        adapter.description = WifiUtils::WideToUtf8(pIfInfo->strInterfaceDescription);
        adapter.manufacturer = WifiUtils::ExtractManufacturer(adapter.description);

        switch (pIfInfo->isState) {
            case wlan_interface_state_not_ready: adapter.state = "Not Ready"; break;
            case wlan_interface_state_connected: adapter.state = "Connected"; adapter.isConnected = true; break;
            case wlan_interface_state_ad_hoc_network_formed: adapter.state = "Ad-Hoc"; break;
            case wlan_interface_state_disconnecting: adapter.state = "Disconnecting"; break;
            case wlan_interface_state_disconnected: adapter.state = "Disconnected"; break;
            case wlan_interface_state_associating: adapter.state = "Associating"; break;
            case wlan_interface_state_discovering: adapter.state = "Discovering"; break;
            case wlan_interface_state_authenticating: adapter.state = "Authenticating"; break;
            default: adapter.state = "Unknown"; break;
        }

        // Read connection attributes if connected
        if (adapter.isConnected) {
            DWORD dwDataSize = 0;
            PWLAN_CONNECTION_ATTRIBUTES pConn = nullptr;
            if (WlanQueryInterface(m_hClient, &pIfInfo->InterfaceGuid, wlan_intf_opcode_current_connection,
                nullptr, &dwDataSize, (PVOID*)&pConn, nullptr) == ERROR_SUCCESS && pConn) {
                
                adapter.connectedSsid = std::string(
                    (char*)pConn->wlanAssociationAttributes.dot11Ssid.ucSSID,
                    pConn->wlanAssociationAttributes.dot11Ssid.uSSIDLength
                );
                adapter.connectedBssid = WifiUtils::MacToString(pConn->wlanAssociationAttributes.dot11Bssid);
                WlanFreeMemory(pConn);
            }
        }

        if (triggerScan) {
            WlanScan(m_hClient, &pIfInfo->InterfaceGuid, nullptr, nullptr, nullptr);
            Sleep(200); // Give hardware time to initiate scan
        }

        adapter.accessPoints = RetrieveAccessPoints(pIfInfo->InterfaceGuid, adapter.connectedBssid);
        adapters.push_back(adapter);
    }

    if (pIfList) WlanFreeMemory(pIfList);
    return adapters;
}
