#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bluetoothapis.h>
#include <string>
#include <vector>

class BluetoothDevice {
public:
    int bus;
    int slot;
    int func;
    std::string macAddress;     // Physical Bluetooth MAC (e.g., a4:83:e7:2b:11:02)
    std::string name;           // Device friendly name
    std::string deviceClass;    // Decoded major/minor class (e.g., Audio/Video)
    std::string hwPath;         // hardware path (e.g., 0/0/2/1.0)
    std::string driver;         // Windows Bluetooth profile driver (e.g., BthA2DP)
    std::string codHex;         // Hex Class of Device (e.g., 0x240404)
    bool isConnected;
    bool isPaired;
    std::string lastSeen;

    BluetoothDevice(int b, int s, int f,
                    std::string mac, std::string devName,
                    std::string cls, std::string path,
                    std::string drv = "BthEnum", std::string cod = "0x000000",
                    bool connected = false, bool paired = true,
                    std::string seen = "Recent");

    std::string getShortSlot() const;
    std::string getPseudoId() const;
    bool matches(int fBus, int fSlot, const std::string& fMac, bool onlyConnected) const;
};
