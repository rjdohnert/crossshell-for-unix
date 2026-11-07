#ifndef DRIVERCTL_SCANNER_HPP
#define DRIVERCTL_SCANNER_HPP

#include "driverctl_models.hpp"

#include <string>
#include <string_view>
#include <vector>

struct ScanResult {
    std::vector<DeviceRecord> devices;
    std::string error;

    explicit operator bool() const {
        return error.empty();
    }
};

class DeviceScanner {
public:
    ScanResult Scan(bool testDrivers, std::string_view categoryFilter);
};

#endif
