// ============================================================================
// 2. DATA MODELS & CONFIGURATION
// ============================================================================

struct DeviceRecord {
    unsigned int index = 0;
    std::wstring className;
    std::wstring hwPath;
    std::wstring instanceId;
    std::wstring hardwareId;
    std::wstring driver;
    std::wstring swState;
    std::wstring hwType;
    std::wstring description;
    std::wstring manufacturer;
    std::wstring location;
    std::wstring health;
    bool present = false;
};

struct IoscanOptions {
    bool fullView = false;
    bool summaryView = false;
    bool agileView = false;
    bool compactView = false;
    bool listDeviceFiles = false;
    bool showAlias = false;
    bool showDevicePath = false;
    bool processorThreads = false;
    bool kernelOnly = false;
    bool presentOnly = true;
    bool usableOnly = false;
    bool localOnly = false;
    bool staleOnly = false;
    bool unclaimedInterfacesOnly = false;
    bool showScanTime = false;
    bool listPendingBindings = false;
    std::wstring classFilter;
    std::wstring driverFilter;
    std::wstring hwPathFilter;
    int instanceFilter = -1;
    std::wstring mappingKeyword;
    std::wstring propertyName;
    int outputFormat = 0; // 0=default/table, 1=json, 2=csv, 3=table
    std::wstring pipeCommand;
};

