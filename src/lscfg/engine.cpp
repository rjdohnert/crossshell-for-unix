#include "engine.hpp"
#include <iomanip>
#include <sstream>

std::string StringHelper::WideToNarrow(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, NULL, NULL);
    return str;
}

std::string StringHelper::VariantToString(const VARIANT& vt) {
    if (vt.vt == VT_NULL || vt.vt == VT_EMPTY) return "N/A";
    if (vt.vt == VT_BSTR) return WideToNarrow(vt.bstrVal);
    if (vt.vt == VT_I4) return std::to_string(vt.lVal);
    if (vt.vt == VT_UI4) return std::to_string(vt.ulVal);
    if (vt.vt == VT_UI8) return std::to_string(vt.ullVal);
    if (vt.vt == VT_BOOL) return (vt.boolVal == VARIANT_TRUE) ? "True" : "False";
    return "N/A";
}

std::string StringHelper::FormatBytes(unsigned long long bytes) {
    if (bytes == 0) return "N/A";
    const char* suffixes[] = { "B", "KB", "MB", "GB", "TB" };
    int i = 0;
    double dBytes = static_cast<double>(bytes);
    while (dBytes >= 1024 && i < 4) {
        dBytes /= 1024;
        i++;
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << dBytes << " " << suffixes[i];
    return ss.str();
}

WmiEngine::WmiEngine() {
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return;

    hr = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL
    );

    hr = CoCreateInstance(
        CLSID_WbemLocator, 0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc
    );

    if (FAILED(hr)) return;

    hr = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        NULL, NULL, 0, NULL, 0, 0, &pSvc
    );

    if (FAILED(hr)) return;

    hr = CoSetProxyBlanket(
        pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE
    );

    if (SUCCEEDED(hr)) initialized = true;
}

WmiEngine::~WmiEngine() {
    if (pSvc) pSvc->Release();
    if (pLoc) pLoc->Release();
    CoUninitialize();
}

bool WmiEngine::IsInitialized() const {
    return initialized;
}

std::vector<std::map<std::string, std::string>> WmiEngine::Query(const std::wstring& wmiClass, const std::vector<std::wstring>& properties) {
    std::vector<std::map<std::string, std::string>> results;
    if (!initialized) return results;

    std::wstring query = L"SELECT ";
    for (size_t i = 0; i < properties.size(); ++i) {
        query += properties[i];
        if (i < properties.size() - 1) query += L", ";
    }
    query += L" FROM " + wmiClass;

    IEnumWbemClassObject* pEnumerator = nullptr;
    HRESULT hr = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t(query.c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator
    );

    if (FAILED(hr) || !pEnumerator) return results;

    IWbemClassObject* pclsObj = nullptr;
    ULONG uReturn = 0;

    while (pEnumerator) {
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) break;

        std::map<std::string, std::string> row;
        for (const auto& prop : properties) {
            VARIANT vtProp;
            VariantInit(&vtProp);
            hr = pclsObj->Get(prop.c_str(), 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr)) {
                row[StringHelper::WideToNarrow(prop)] = StringHelper::VariantToString(vtProp);
            }
            VariantClear(&vtProp);
        }
        results.push_back(row);
        pclsObj->Release();
    }
    pEnumerator->Release();
    return results;
}

std::vector<HardwareDevice> InventoryCollector::Collect(WmiEngine& wmi) {
    std::vector<HardwareDevice> devices;

    // 1. System / BIOS / BaseBoard
    auto sysList = wmi.Query(L"Win32_ComputerSystem", { L"Manufacturer", L"Model", L"SystemType" });
    auto biosList = wmi.Query(L"Win32_BIOS", { L"Manufacturer", L"Name", L"SMBIOSBIOSVersion", L"SerialNumber", L"ReleaseDate" });
    auto boardList = wmi.Query(L"Win32_BaseBoard", { L"Manufacturer", L"Product", L"SerialNumber" });

    HardwareDevice sysDev;
    sysDev.resourceName = "sys0";
    sysDev.location = "System Planar";
    sysDev.deviceClass = "sys";
    sysDev.description = (!sysList.empty()) ? (sysList[0]["Manufacturer"] + " " + sysList[0]["Model"]) : "Windows Workstation System";
    if (!sysList.empty()) {
        sysDev.vpdAttributes.push_back({ "System Model", sysList[0]["Model"] });
        sysDev.vpdAttributes.push_back({ "System Architecture", sysList[0]["SystemType"] });
    }
    if (!biosList.empty()) {
        sysDev.vpdAttributes.push_back({ "BIOS Vendor", biosList[0]["Manufacturer"] });
        sysDev.vpdAttributes.push_back({ "BIOS Version", biosList[0]["SMBIOSBIOSVersion"] });
        sysDev.vpdAttributes.push_back({ "System Serial Number", biosList[0]["SerialNumber"] });
    }
    if (!boardList.empty()) {
        sysDev.vpdAttributes.push_back({ "Motherboard Product", boardList[0]["Product"] });
        sysDev.vpdAttributes.push_back({ "Motherboard Serial", boardList[0]["SerialNumber"] });
    }
    devices.push_back(sysDev);

    // 2. Processors (CPU)
    auto cpuList = wmi.Query(L"Win32_Processor", { L"DeviceID", L"Name", L"Manufacturer", L"NumberOfCores", L"NumberOfLogicalProcessors", L"MaxClockSpeed", L"SocketDesignation" });
    int cpuIdx = 0;
    for (const auto& c : cpuList) {
        HardwareDevice dev;
        dev.resourceName = "proc" + std::to_string(cpuIdx++);
        dev.location = (c.at("SocketDesignation") != "N/A") ? c.at("SocketDesignation") : "CPU Socket";
        dev.description = c.at("Name");
        dev.deviceClass = "cpu";
        dev.vpdAttributes.push_back({ "Manufacturer", c.at("Manufacturer") });
        dev.vpdAttributes.push_back({ "Physical Cores", c.at("NumberOfCores") });
        dev.vpdAttributes.push_back({ "Logical Threads", c.at("NumberOfLogicalProcessors") });
        dev.vpdAttributes.push_back({ "Max Clock Speed", c.at("MaxClockSpeed") + " MHz" });
        devices.push_back(dev);
    }

    // 3. Physical Memory (RAM)
    auto memList = wmi.Query(L"Win32_PhysicalMemory", { L"BankLabel", L"DeviceLocator", L"Capacity", L"Speed", L"Manufacturer", L"PartNumber", L"SerialNumber" });
    int memIdx = 0;
    for (const auto& m : memList) {
        HardwareDevice dev;
        dev.resourceName = "mem" + std::to_string(memIdx++);
        dev.location = (m.at("DeviceLocator") != "N/A") ? m.at("DeviceLocator") : ("Slot " + m.at("BankLabel"));
        unsigned long long cap = 0;
        try { cap = std::stoull(m.at("Capacity")); } catch (...) {}
        dev.description = StringHelper::FormatBytes(cap) + " Physical Memory Module";
        dev.deviceClass = "mem";
        dev.vpdAttributes.push_back({ "Manufacturer", m.at("Manufacturer") });
        dev.vpdAttributes.push_back({ "Part Number", m.at("PartNumber") });
        dev.vpdAttributes.push_back({ "Serial Number", m.at("SerialNumber") });
        dev.vpdAttributes.push_back({ "Configured Speed", m.at("Speed") + " MT/s" });
        devices.push_back(dev);
    }

    // 4. Storage Disks
    auto diskList = wmi.Query(L"Win32_DiskDrive", { L"DeviceID", L"Model", L"InterfaceType", L"Size", L"SerialNumber", L"MediaType" });
    int diskIdx = 0;
    for (const auto& d : diskList) {
        HardwareDevice dev;
        dev.resourceName = "hdisk" + std::to_string(diskIdx++);
        dev.location = d.at("InterfaceType") + " Controller";
        unsigned long long size = 0;
        try { size = std::stoull(d.at("Size")); } catch (...) {}
        dev.description = d.at("Model") + " (" + StringHelper::FormatBytes(size) + ")";
        dev.deviceClass = "disk";
        dev.vpdAttributes.push_back({ "Model / Machine Type", d.at("Model") });
        dev.vpdAttributes.push_back({ "Serial Number", d.at("SerialNumber") });
        dev.vpdAttributes.push_back({ "Bus Interface", d.at("InterfaceType") });
        dev.vpdAttributes.push_back({ "Media Type", d.at("MediaType") });
        devices.push_back(dev);
    }

    // 5. Network Adapters
    auto netList = wmi.Query(L"Win32_NetworkAdapter", { L"Name", L"AdapterType", L"MACAddress", L"Manufacturer", L"PNPDeviceID" });
    int netIdx = 0;
    for (const auto& n : netList) {
        if (n.at("MACAddress") == "N/A" || n.at("Name").find("Virtual") != std::string::npos) continue;
        HardwareDevice dev;
        dev.resourceName = "ent" + std::to_string(netIdx++);
        dev.location = "PCI Bus Adapter";
        dev.description = n.at("Name");
        dev.deviceClass = "net";
        dev.vpdAttributes.push_back({ "Manufacturer", n.at("Manufacturer") });
        dev.vpdAttributes.push_back({ "Network Address (MAC)", n.at("MACAddress") });
        dev.vpdAttributes.push_back({ "PNP Device ID", n.at("PNPDeviceID") });
        devices.push_back(dev);
    }

    // 6. GPUs / Display Controllers
    auto gpuList = wmi.Query(L"Win32_VideoController", { L"Name", L"DriverVersion", L"AdapterRAM", L"VideoProcessor" });
    int gpuIdx = 0;
    for (const auto& g : gpuList) {
        HardwareDevice dev;
        dev.resourceName = "gpu" + std::to_string(gpuIdx++);
        dev.location = "PCIe Graphics Slot";
        dev.description = g.at("Name");
        dev.deviceClass = "gpu";
        dev.vpdAttributes.push_back({ "Video Processor", g.at("VideoProcessor") });
        dev.vpdAttributes.push_back({ "Driver Version", g.at("DriverVersion") });
        unsigned long long vram = 0;
        try { vram = std::stoull(g.at("AdapterRAM")); } catch (...) {}
        dev.vpdAttributes.push_back({ "VRAM Capacity", StringHelper::FormatBytes(vram) });
        devices.push_back(dev);
    }

    return devices;
}
