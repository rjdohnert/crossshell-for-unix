// Result of driver status testing
enum class HealthStatus {
    Healthy,
    Warning,
    Malfunction,
    Disabled
};

// Maps a HealthStatus to a stable lowercase string for machine-readable output.
std::string HealthToString(HealthStatus h) {
    switch (h) {
        case HealthStatus::Healthy:     return "healthy";
        case HealthStatus::Warning:     return "warning";
        case HealthStatus::Malfunction: return "malfunction";
        case HealthStatus::Disabled:    return "disabled";
    }
    return "unknown";
}

// Represents an inspected device and its driver metadata
struct DeviceRecord {
    std::string deviceName;
    std::string category;
    std::string driverType;
    std::string driverProvider;
    std::string driverVersion;
    std::wstring serviceName;            // kept native (wide) to avoid a UTF-8 round-trip
    std::vector<std::string> upperFilters; // DEVPKEY_Device_UpperFilters
    std::vector<std::string> lowerFilters; // DEVPKEY_Device_LowerFilters
    HealthStatus health = HealthStatus::Healthy;
    ULONG problemCode = 0;
    std::string diagnosticDetails;
};
