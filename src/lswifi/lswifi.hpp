#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wlanapi.h>
#include <wlantypes.h>
#include <string>
#include <vector>
#include <cstdint>

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
