#include "reporter.hpp"
#include "engine.hpp"
#include <iostream>
#include <iomanip>

std::string TableFormatter::RenderSignalBar(uint32_t quality, bool color) {
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

void TableFormatter::Output(const std::vector<WifiAdapter>& adapters, const RuntimeConfig& cfg) {
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

void JsonFormatter::Output(const std::vector<WifiAdapter>& adapters, const RuntimeConfig& cfg) {
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

void CsvFormatter::Output(const std::vector<WifiAdapter>& adapters, const RuntimeConfig& cfg) {
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

std::unique_ptr<IFormatter> FormatterFactory::Create(RuntimeConfig::Format format) {
    switch (format) {
        case RuntimeConfig::Format::Json: return std::make_unique<JsonFormatter>();
        case RuntimeConfig::Format::Csv:  return std::make_unique<CsvFormatter>();
        default:                          return std::make_unique<TableFormatter>();
    }
}
