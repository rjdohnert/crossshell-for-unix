/*
BSD 3 Clause License
--------------------

CrossShell for UNIX
Copyright (c) 2026, Roberto J Dohnert
All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions, and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions, and the following disclaimer
in the documentation and/or other materials provided with the distribution. Neither the name of [project] nor the names of its
contributors may be used to endorse or promote products derived from this software without specific prior written permission.

Disclaimer:
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAG
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wlanapi.h>
#include <wlantypes.h>
#include <io.h>

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <memory>
#include <algorithm>
#include <optional>

#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")

// ============================================================================
// Data Models
// ============================================================================
struct AccessPoint {
    std::string bssid;
    std::string ssid;
    int rssiDbm = 0;
    uint32_t signalQuality = 0; // 0 - 100%
    uint32_t channel = 0;
    uint32_t frequencyKhz = 0;
    std::string band;           // 2.4 GHz, 5 GHz, 6 GHz
    std::string authAlgorithm;
    std::string cipherAlgorithm;
    bool isConnected = false;
};

struct WifiAdapter {
    std::string guid;
    std::string description;
    std::string manufacturer;
    std::string state;
    bool isConnected = false;
    std::string connectedSsid;
    std::string connectedBssid;
    std::vector<AccessPoint> accessPoints;
};

// ============================================================================
// Utility & Helper Functions
// ============================================================================
class WifiUtils {
public:
    static std::string GuidToString(const GUID& guid) {
        char buf[40];
        snprintf(buf, sizeof(buf), "{%08lX-%04hX-%04hX-%02X%02X-%02X%02X%02X%02X%02X%02X}",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        return std::string(buf);
    }

    static std::string MacToString(const unsigned char* mac) {
        char buf[18];
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return std::string(buf);
    }

    static std::string ExtractManufacturer(const std::string& desc) {
        static const std::vector<std::string> vendors = {
            "Intel", "Realtek", "Broadcom", "Qualcomm", "MediaTek",
            "Atheros", "TP-Link", "Killer", "Marvell", "Ralink", "ASUS"
        };
        for (const auto& v : vendors) {
            if (desc.find(v) != std::string::npos) return v;
        }
        return "Generic / Other";
    }

    static std::string WideToUtf8(const std::wstring& wide) {
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

    static std::pair<uint32_t, std::string> FreqToChannelAndBand(uint32_t freqKhz) {
        uint32_t freqMhz = freqKhz / 1000;
        if (freqMhz == 2484) return { 14, "2.4 GHz" };
        if (freqMhz >= 2412 && freqMhz <= 2472) return { (freqMhz - 2407) / 5, "2.4 GHz" };
        if (freqMhz >= 5170 && freqMhz <= 5825) return { (freqMhz - 5000) / 5, "5 GHz" };
        if (freqMhz >= 5955 && freqMhz <= 7115) return { (freqMhz - 5950) / 5, "6 GHz" };
        return { 0, "Unknown" };
    }

    static std::string AuthToString(DOT11_AUTH_ALGORITHM auth) {
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

    static std::string CipherToString(DOT11_CIPHER_ALGORITHM cipher) {
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

    static std::string EscapeJson(const std::string& s) {
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
};

// ============================================================================
// Core Wi-Fi Manager (RAII Wrapper around WlanApi)
// ============================================================================
class WifiManager {
private:
    HANDLE m_hClient = nullptr;
    DWORD m_negotiatedVersion = 0;

public:
    WifiManager() {
        DWORD dwResult = WlanOpenHandle(2, nullptr, &m_negotiatedVersion, &m_hClient);
        if (dwResult != ERROR_SUCCESS) {
            throw std::runtime_error("Failed to open WLAN Handle. (Error Code: " + std::to_string(dwResult) + ")");
        }
    }

    ~WifiManager() {
        if (m_hClient) {
            WlanCloseHandle(m_hClient, nullptr);
            m_hClient = nullptr;
        }
    }

    std::vector<WifiAdapter> EnumerateAdapters(bool triggerScan = false) {
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

private:
    std::vector<AccessPoint> RetrieveAccessPoints(const GUID& guid, const std::string& currentBssid) {
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
};

// ============================================================================
// Filter and Execution Config
// ============================================================================
struct RuntimeConfig {
    enum class Format { Table, Json, Csv } format = Format::Table;
    bool showAdapters = true;
    bool showNetworks = true;
    bool triggerScan = false;
    bool useColor = true;
    int minSignal = 0;
    std::string ssidFilter = "";
    std::string bandFilter = "";
};

// ============================================================================
// Formatter Strategy Pattern
// ============================================================================
class IFormatter {
public:
    virtual ~IFormatter() = default;
    virtual void Output(const std::vector<WifiAdapter>& adapters, const RuntimeConfig& cfg) = 0;
};

class TableFormatter : public IFormatter {
public:
    void Output(const std::vector<WifiAdapter>& adapters, const RuntimeConfig& cfg) override {
        const std::string CLR_RESET  = cfg.useColor ? "\033[0m" : "";
        const std::string CLR_BOLD   = cfg.useColor ? "\033[1m" : "";
        const std::string CLR_CYAN   = cfg.useColor ? "\033[36m" : "";
        const std::string CLR_GREEN  = cfg.useColor ? "\033[32m" : "";
        const std::string CLR_YELLOW = cfg.useColor ? "\033[33m" : "";
        const std::string CLR_GRAY   = cfg.useColor ? "\033[90m" : "";

        if (cfg.showAdapters) {
            std::cout << CLR_BOLD << CLR_CYAN << "\nWireless Networks" << CLR_RESET << "\n\n";
            std::cout << std::left 
                      << std::setw(28) << "Manufacturer" 
                      << std::setw(15) << "State" 
                      << std::setw(22) << "Connected SSID" 
                      << std::setw(20) << "Connected BSSID" 
                      << "Device Description" << "\n";
            std::cout << std::string(110, '-') << "\n";

            for (const auto& ad : adapters) {
                std::string status = (ad.isConnected ? CLR_GREEN : CLR_GRAY) + ad.state + CLR_RESET;
                std::cout << std::left 
                          << std::setw(28) << ad.manufacturer
                          << std::setw(24) << status 
                          << std::setw(22) << (ad.connectedSsid.empty() ? "-" : ad.connectedSsid)
                          << std::setw(20) << (ad.connectedBssid.empty() ? "-" : ad.connectedBssid)
                          << ad.description << "\n";
            }
            std::cout << "\n";
        }

        if (cfg.showNetworks) {
            std::cout << CLR_BOLD << CLR_CYAN << "\nVisible Access Points" << CLR_RESET << "\n\n";
            std::cout << std::left 
                      << std::setw(4)  << "Use"
                      << std::setw(28) << "SSID" 
                      << std::setw(20) << "BSSID" 
                      << std::setw(8)  << "Sig (%)" 
                      << std::setw(8)  << "RSSI" 
                      << std::setw(6)  << "Chan" 
                      << std::setw(10) << "Band" 
                      << "Quality Bar\n";
            std::cout << std::string(98, '-') << "\n";

            for (const auto& ad : adapters) {
                if (adapters.size() > 1) {
                    std::cout << CLR_YELLOW << "Interface: " << ad.description << CLR_RESET << "\n";
                }
                for (const auto& ap : ad.accessPoints) {
                    if (ap.signalQuality < (uint32_t)cfg.minSignal) continue;
                    if (!cfg.ssidFilter.empty() && ap.ssid.find(cfg.ssidFilter) == std::string::npos) continue;
                    if (!cfg.bandFilter.empty() && ap.band.find(cfg.bandFilter) == std::string::npos) continue;

                    std::string inUseIndicator = ap.isConnected ? (CLR_GREEN + " [*]" + CLR_RESET) : "    ";
                    std::string bar = RenderSignalBar(ap.signalQuality, cfg.useColor);

                    std::cout << std::left 
                              << std::setw(4)  << inUseIndicator
                              << std::setw(28) << (ap.ssid.length() > 27 ? ap.ssid.substr(0, 24) + "..." : ap.ssid)
                              << std::setw(20) << ap.bssid
                              << std::setw(8)  << (std::to_string(ap.signalQuality) + "%")
                              << std::setw(8)  << (std::to_string(ap.rssiDbm) + "dBm")
                              << std::setw(6)  << ap.channel
                              << std::setw(10) << ap.band
                              << bar << "\n";
                }
            }
            std::cout << "\n";
        }
    }

private:
    static std::string RenderSignalBar(uint32_t quality, bool color) {
        int totalBars = 10;
        int activeBars = (quality + 9) / 10;
        std::string bar = "[";
        for (int i = 0; i < totalBars; ++i) {
            if (i < activeBars) bar += "#";
            else bar += " ";
        }
        bar += "]";

        if (!color) return bar;
        if (quality >= 75) return "\033[32m" + bar + "\033[0m"; // Green
        if (quality >= 40) return "\033[33m" + bar + "\033[0m"; // Yellow
        return "\033[31m" + bar + "\033[0m";                   // Red
    }
};

class JsonFormatter : public IFormatter {
public:
    void Output(const std::vector<WifiAdapter>& adapters, const RuntimeConfig& cfg) override {
        std::cout << "{\n  \"adapters\": [\n";
        for (size_t i = 0; i < adapters.size(); ++i) {
            const auto& ad = adapters[i];
            std::cout << "    {\n";
            std::cout << "      \"guid\": \"" << ad.guid << "\",\n";
            std::cout << "      \"description\": \"" << WifiUtils::EscapeJson(ad.description) << "\",\n";
            std::cout << "      \"manufacturer\": \"" << WifiUtils::EscapeJson(ad.manufacturer) << "\",\n";
            std::cout << "      \"state\": \"" << ad.state << "\",\n";
            std::cout << "      \"isConnected\": " << (ad.isConnected ? "true" : "false") << ",\n";
            std::cout << "      \"connectedSsid\": \"" << WifiUtils::EscapeJson(ad.connectedSsid) << "\",\n";
            std::cout << "      \"connectedBssid\": \"" << ad.connectedBssid << "\",\n";
            std::cout << "      \"accessPoints\": [\n";

            size_t written = 0;
            for (size_t j = 0; j < ad.accessPoints.size(); ++j) {
                const auto& ap = ad.accessPoints[j];
                if (ap.signalQuality < (uint32_t)cfg.minSignal) continue;
                if (!cfg.ssidFilter.empty() && ap.ssid.find(cfg.ssidFilter) == std::string::npos) continue;
                if (!cfg.bandFilter.empty() && ap.band.find(cfg.bandFilter) == std::string::npos) continue;

                if (written > 0) std::cout << ",\n";
                std::cout << "        {\n";
                std::cout << "          \"ssid\": \"" << WifiUtils::EscapeJson(ap.ssid) << "\",\n";
                std::cout << "          \"bssid\": \"" << ap.bssid << "\",\n";
                std::cout << "          \"rssiDbm\": " << ap.rssiDbm << ",\n";
                std::cout << "          \"signalQuality\": " << ap.signalQuality << ",\n";
                std::cout << "          \"channel\": " << ap.channel << ",\n";
                std::cout << "          \"frequencyKhz\": " << ap.frequencyKhz << ",\n";
                std::cout << "          \"band\": \"" << ap.band << "\",\n";
                std::cout << "          \"inUse\": " << (ap.isConnected ? "true" : "false") << "\n";
                std::cout << "        }";
                written++;
            }
            std::cout << "\n      ]\n    }" << (i + 1 < adapters.size() ? "," : "") << "\n";
        }
        std::cout << "  ]\n}\n";
    }
};

class CsvFormatter : public IFormatter {
public:
    void Output(const std::vector<WifiAdapter>& adapters, const RuntimeConfig& cfg) override {
        std::cout << "AdapterManufacturer,AdapterDescription,AdapterState,SSID,BSSID,SignalQuality,RssiDbm,Channel,Band,InUse\n";
        for (const auto& ad : adapters) {
            for (const auto& ap : ad.accessPoints) {
                if (ap.signalQuality < (uint32_t)cfg.minSignal) continue;
                if (!cfg.ssidFilter.empty() && ap.ssid.find(cfg.ssidFilter) == std::string::npos) continue;
                if (!cfg.bandFilter.empty() && ap.band.find(cfg.bandFilter) == std::string::npos) continue;

                std::cout << "\"" << ad.manufacturer << "\","
                          << "\"" << ad.description << "\","
                          << "\"" << ad.state << "\","
                          << "\"" << ap.ssid << "\","
                          << "\"" << ap.bssid << "\","
                          << ap.signalQuality << ","
                          << ap.rssiDbm << ","
                          << ap.channel << ","
                          << "\"" << ap.band << "\","
                          << (ap.isConnected ? "true" : "false") << "\n";
            }
        }
    }
};

// ============================================================================
// Command-Line Parsing & CLI Flow
// ============================================================================
class CommandLineApp {
public:
    static void PrintHelp() {
        std::cout << R"(lswifi(1)                CrossShell for UNIX Reference Manual                 lswifi(1)

    NAME
        lswifi - lists Wi-Fi adapters and nearby wireless access points

    SYNOPSIS
        lswifi [OPTIONS]

    DESCRIPTION
        Lists Wi-Fi adapters and nearby access points, including connection state,
        SSID, BSSID, signal strength, band, channel, and link quality metrics.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -a, --adapters-only
            Display Wi-Fi adapters and connected status only.

        -n, --networks-only
            Display visible access points and networks only.

        -s, --scan
            Trigger an active Wi-Fi hardware scan before reading.

        -f, --format <format>
            Select output format: table (default), json, or csv.

        --min-signal <quality>
            Filter out networks weaker than the specified quality percentage (0-100).

        --ssid <filter>
            Filter networks matching substring (case-sensitive).

        --band <band>
            Filter networks by band ('2.4', '5', or '6').

        --no-color
            Force-disable ANSI terminal color escapes.

        -h, --help
            Display this comprehensive reference manual and exit.

        -V, --version
            Display version information and exit.

    EXAMPLES
        lswifi
            List all Wi-Fi adapters and visible access points in table format.

        lswifi -s --format json
            Trigger an active scan and output results in JSON format.

        lswifi --min-signal 60 --band 5
            Display 5 GHz networks with at least 60% signal quality.

        lswifi --format csv > scan_results.csv
            Export scan results to a CSV file.

    EXIT STATUS
        0
            Success.
        1
            Error communicating with the WLAN service or invalid arguments.

    CrossShell for UNIX                                                    lswifi(1)
)";
    }

    static int Run(int argc, char* argv[]) {
        RuntimeConfig cfg;

        // Auto-detect redirection / pipes to turn off ANSI formatting
        if (!_isatty(_fileno(stdout))) {
            cfg.useColor = false;
        }

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help" || arg == "/?") {
                PrintHelp();
                return 0;
            } else if (arg == "-V" || arg == "--version") {
                std::cout << "lswifi 1.0.0\n";
                return 0;
            } else if (arg == "-f" || arg == "--format") {
                if (++i < argc) {
                    std::string fmt = argv[i];
                    if (fmt == "json") cfg.format = RuntimeConfig::Format::Json;
                    else if (fmt == "csv") cfg.format = RuntimeConfig::Format::Csv;
                    else cfg.format = RuntimeConfig::Format::Table;
                }
            } else if (arg == "-s" || arg == "--scan") {
                cfg.triggerScan = true;
            } else if (arg == "-a" || arg == "--adapters-only") {
                cfg.showAdapters = true;
                cfg.showNetworks = false;
            } else if (arg == "-n" || arg == "--networks-only") {
                cfg.showAdapters = false;
                cfg.showNetworks = true;
            } else if (arg == "--min-signal") {
                if (++i < argc) cfg.minSignal = std::stoi(argv[i]);
            } else if (arg == "--ssid") {
                if (++i < argc) cfg.ssidFilter = argv[i];
            } else if (arg == "--band") {
                if (++i < argc) cfg.bandFilter = argv[i];
            } else if (arg == "--no-color") {
                cfg.useColor = false;
            }
        }

        // Enable Windows ANSI escape codes if writing to a real console
        if (cfg.useColor) {
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }

        try {
            WifiManager mgr;
            auto adapters = mgr.EnumerateAdapters(cfg.triggerScan);

            std::unique_ptr<IFormatter> formatter;
            switch (cfg.format) {
                case RuntimeConfig::Format::Json: formatter = std::make_unique<JsonFormatter>(); break;
                case RuntimeConfig::Format::Csv:  formatter = std::make_unique<CsvFormatter>(); break;
                default:                          formatter = std::make_unique<TableFormatter>(); break;
            }

            formatter->Output(adapters, cfg);
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << "\n";
            return 1;
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    return CommandLineApp::Run(argc, argv);
}