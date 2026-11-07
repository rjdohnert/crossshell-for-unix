#ifndef DRIVERCTL_MODELS_HPP
#define DRIVERCTL_MODELS_HPP

#include <string>
#include <vector>

enum class HealthStatus {
    Healthy,
    Warning,
    Malfunction,
    Disabled
};

inline std::string HealthToString(HealthStatus status) {
    switch (status) {
        case HealthStatus::Healthy:     return "healthy";
        case HealthStatus::Warning:     return "warning";
        case HealthStatus::Malfunction: return "malfunction";
        case HealthStatus::Disabled:    return "disabled";
    }
    return "unknown";
}

struct DeviceRecord {
    std::string deviceName;
    std::string category;
    std::string driverType;
    std::string driverProvider;
    std::string driverVersion;
    std::wstring serviceName;
    std::vector<std::string> upperFilters;
    std::vector<std::string> lowerFilters;
    HealthStatus health = HealthStatus::Healthy;
    unsigned long problemCode = 0;
    std::string diagnosticDetails;
};

#endif
