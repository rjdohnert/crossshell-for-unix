/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * ============================================================================
 * SINGLE FILE INDEX: lsdev.cpp
 * ============================================================================
 * WinLsdev - Object-Oriented Windows Device Node & Class Enumerator
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [DATA MODEL & OPTIONS] ................ Device, CommandLineOptions
 * 2. [SETUPAPI ENUMERATION] ................ IDeviceEnumerator, SetupApiDeviceEnumerator
 * 3. [FILTERING & OUTPUT] .................. DeviceFilter, IDeviceRenderer implementations
 * 4. [APPLICATION CONTROLLER] .............. LsDevApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

// ============================================================================
// Helper Utilities
// ============================================================================
namespace StringUtil {
    std::string ToLower(const std::string& str) {
        std::string result(str);
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    std::string WideToAnsi(const std::wstring& wstr) {
        if (wstr.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], size, nullptr, nullptr);
        return str;
    }

    std::vector<std::string> Split(const std::string& str, char delim) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream stream(str);
        while (std::getline(stream, token, delim)) {
            tokens.push_back(token);
        }
        return tokens;
    }
}

// ============================================================================
// Device Representation (Object-Oriented Entity)
// ============================================================================
class Device {
public:
    Device(std::string logicalName,
           std::string state,
           std::string location,
           std::string description,
           std::string devClass,
           std::string subClass,
           std::string devType)
        : m_logicalName(std::move(logicalName)),
          m_state(std::move(state)),
          m_location(std::move(location)),
          m_description(std::move(description)),
          m_class(std::move(devClass)),
          m_subClass(std::move(subClass)),
          m_type(std::move(devType)) {}

    [[nodiscard]] const std::string& GetName() const noexcept { return m_logicalName; }
    [[nodiscard]] const std::string& GetState() const noexcept { return m_state; }
    [[nodiscard]] const std::string& GetLocation() const noexcept { return m_location; }
    [[nodiscard]] const std::string& GetDescription() const noexcept { return m_description; }
    [[nodiscard]] const std::string& GetClass() const noexcept { return m_class; }
    [[nodiscard]] const std::string& GetSubClass() const noexcept { return m_subClass; }
    [[nodiscard]] const std::string& GetType() const noexcept { return m_type; }

    [[nodiscard]] std::string GetField(const std::string& fieldName) const {
        std::string f = StringUtil::ToLower(fieldName);
        if (f == "name") return m_logicalName;
        if (f == "status" || f == "state") return m_state;
        if (f == "location" || f == "loc") return m_location;
        if (f == "description" || f == "desc") return m_description;
        if (f == "class") return m_class;
        if (f == "subclass") return m_subClass;
        if (f == "type") return m_type;
        return "";
    }

private:
    std::string m_logicalName;
    std::string m_state;
    std::string m_location;
    std::string m_description;
    std::string m_class;
    std::string m_subClass;
    std::string m_type;
};

// ============================================================================
// Command Line Options Parser
// ============================================================================
struct CommandLineOptions {
    bool customizedMode = true;   // -C (default)
    bool predefinedMode = false;  // -P
    bool showHeaders = false;     // -H
    bool showHelp = false;        // -h, --help

    std::string filterClass;      // -c <Class>
    std::string filterSubclass;   // -s <Subclass>
    std::string filterType;       // -t <Type>
    std::string filterName;       // -l <Name>
    std::string filterState;      // -S <State>
    std::string customFormat;     // -F <Format>
    std::string listColumn;       // -r <ColumnName>
};

class ArgumentParser {
public:
    [[nodiscard]] CommandLineOptions Parse(int argc, char* argv[]) const {
        CommandLineOptions opts;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                opts.showHelp = true;
            } else if (arg == "-C") {
                opts.customizedMode = true;
                opts.predefinedMode = false;
            } else if (arg == "-P") {
                opts.predefinedMode = true;
                opts.customizedMode = false;
            } else if (arg == "-H") {
                opts.showHeaders = true;
            } else if (arg == "-c" && i + 1 < argc) {
                opts.filterClass = argv[++i];
            } else if (arg == "-s" && i + 1 < argc) {
                opts.filterSubclass = argv[++i];
            } else if (arg == "-t" && i + 1 < argc) {
                opts.filterType = argv[++i];
            } else if (arg == "-l" && i + 1 < argc) {
                opts.filterName = argv[++i];
            } else if (arg == "-S" && i + 1 < argc) {
                opts.filterState = argv[++i];
            } else if (arg == "-F" && i + 1 < argc) {
                opts.customFormat = argv[++i];
            } else if (arg == "-r" && i + 1 < argc) {
                opts.listColumn = argv[++i];
            } else {
                std::cerr << "lsdev: 0514-512 Invalid flag or missing parameter: " << arg << "\n";
                std::cerr << "Try 'lsdev --help' for more information.\n";
                std::exit(1);
            }
        }
        return opts;
    }
};

// ============================================================================
// Windows Device Enumerator (SetupAPI & Configuration Manager Bridge)
// ============================================================================
class IDeviceEnumerator {
public:
    virtual ~IDeviceEnumerator() = default;
    virtual std::vector<Device> EnumerateDevices(bool includeNonPresent) = 0;
};

class SetupApiDeviceEnumerator final : public IDeviceEnumerator {
public:
    std::vector<Device> EnumerateDevices(bool includeNonPresent) override {
        std::vector<Device> devices;
        std::map<std::string, int> classPrefixCounters;

        DWORD flags = DIGCF_ALLCLASSES;
        if (!includeNonPresent) {
            flags |= DIGCF_PRESENT;
        }

        HDEVINFO devInfo = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, flags);
        if (devInfo == INVALID_HANDLE_VALUE) {
            return devices;
        }

        // RAII handle cleanup
        auto cleanup = [](HDEVINFO* h) { if (*h != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(*h); };
        std::unique_ptr<HDEVINFO, decltype(cleanup)> devInfoPtr(&devInfo, cleanup);

        SP_DEVINFO_DATA devData{};
        devData.cbSize = sizeof(SP_DEVINFO_DATA);

        for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devData); ++i) {
            std::string className = GetDeviceProperty(devInfo, devData, SPDRP_CLASS);
            std::string desc = GetDeviceProperty(devInfo, devData, SPDRP_FRIENDLYNAME);
            if (desc.empty()) {
                desc = GetDeviceProperty(devInfo, devData, SPDRP_DEVICEDESC);
            }
            if (desc.empty()) continue; // Skip unconfigured null devices

            std::string enumerator = GetDeviceProperty(devInfo, devData, SPDRP_ENUMERATOR_NAME);
            std::string location = GetDeviceProperty(devInfo, devData, SPDRP_LOCATION_INFORMATION);
            if (location.empty()) {
                location = GetDeviceProperty(devInfo, devData, SPDRP_LOCATION_PATHS);
            }
            // Sanitize location into AIX-style slot/bus coordinates if possible
            if (location.find('#') != std::string::npos) {
                location = location.substr(0, location.find('#'));
            }

            // Determine status
            ULONG status = 0, problem = 0;
            std::string state = "Available";
            if (CR_SUCCESS == CM_Get_DevNode_Status(&status, &problem, devData.DevInst, 0)) {
                if (status & DN_HAS_PROBLEM) {
                    state = (problem == CM_PROB_DISABLED) ? "Defined" : "Stopped";
                } else if (!(status & DN_STARTED)) {
                    state = "Defined";
                }
            } else {
                state = "Defined";
            }

            // Synthesize authentic AIX logical name: hdisk0, ent0, disp0, proc0, etc.
            std::string prefix = MapClassToPrefix(className);
            int idx = classPrefixCounters[prefix]++;
            std::string logicalName = prefix + std::to_string(idx);

            std::string subClass = enumerator.empty() ? "sys" : enumerator;
            std::string devType = className.empty() ? "system" : className;

            devices.emplace_back(logicalName, state, location, desc, className, subClass, devType);
        }

        return devices;
    }

private:
    static std::string GetDeviceProperty(HDEVINFO devInfo, SP_DEVINFO_DATA& devData, DWORD property) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceRegistryPropertyW(devInfo, &devData, property, nullptr, nullptr, 0, &requiredSize);
        if (requiredSize == 0) return {};

        std::vector<BYTE> buffer(requiredSize);
        if (SetupDiGetDeviceRegistryPropertyW(devInfo, &devData, property, nullptr, buffer.data(), requiredSize, nullptr)) {
            return StringUtil::WideToAnsi(reinterpret_cast<const wchar_t*>(buffer.data()));
        }
        return {};
    }

    static std::string MapClassToPrefix(const std::string& className) {
        std::string c = StringUtil::ToLower(className);
        if (c == "diskdrive") return "hdisk";
        if (c == "net") return "ent";
        if (c == "display") return "disp";
        if (c == "processor") return "proc";
        if (c == "system") return "sys";
        if (c == "usb" || c == "usbdevice") return "usb";
        if (c == "scsiadapter" || c == "hdcontroller") return "scsi";
        if (c == "keyboard") return "kbd";
        if (c == "mouse") return "mouse";
        if (c == "media" || c == "audioendpoint") return "audio";
        return "dev";
    }
};

// ============================================================================
// Device Filtering
// ============================================================================
class DeviceFilter {
public:
    explicit DeviceFilter(const CommandLineOptions& options) : m_options(options) {}

    [[nodiscard]] bool IncludesNonPresentDevices() const {
        return !m_options.filterState.empty() &&
               StringUtil::ToLower(m_options.filterState) != "available";
    }

    [[nodiscard]] std::vector<Device> Apply(const std::vector<Device>& devices) const {
        std::vector<Device> filtered;
        filtered.reserve(devices.size());

        for (const auto& device : devices) {
            if (Matches(device)) filtered.push_back(device);
        }
        return filtered;
    }

private:
    [[nodiscard]] bool Matches(const Device& device) const {
        if (!m_options.filterName.empty() &&
            StringUtil::ToLower(device.GetName()) != StringUtil::ToLower(m_options.filterName))
            return false;
        if (!m_options.filterClass.empty() &&
            StringUtil::ToLower(device.GetClass()) != StringUtil::ToLower(m_options.filterClass))
            return false;
        if (!m_options.filterSubclass.empty() &&
            StringUtil::ToLower(device.GetSubClass()) != StringUtil::ToLower(m_options.filterSubclass))
            return false;
        if (!m_options.filterType.empty() &&
            StringUtil::ToLower(device.GetType()) != StringUtil::ToLower(m_options.filterType))
            return false;
        if (!m_options.filterState.empty() &&
            StringUtil::ToLower(device.GetState()) != StringUtil::ToLower(m_options.filterState))
            return false;
        return true;
    }

    const CommandLineOptions& m_options;
};

// ============================================================================
// Formatter and View Layer
// ============================================================================
class IDeviceRenderer {
public:
    virtual ~IDeviceRenderer() = default;
    virtual void ShowHelp() const = 0;
    virtual void Render(const std::vector<Device>& devices) const = 0;
};

class ConsoleDeviceRenderer final : public IDeviceRenderer {
public:
    explicit ConsoleDeviceRenderer(const CommandLineOptions& options) : m_options(options) {}

    void ShowHelp() const override {
        std::cout << "Usage:\n"
                  << "  lsdev -C [-c Class] [-s Subclass] [-t Type] [-l Name] [-S State] [-F Format] [-H]\n"
                  << "  lsdev -P [-c Class] [-s Subclass] [-t Type] [-r Column] [-F Format] [-H]\n"
                  << "  lsdev -r Column [-C | -P]\n"
                  << "  lsdev [-h | --help]\n\n"
                  << "Description:\n"
                  << "  Displays device information and status for the host operating system.\n"
                  << "\n"
                  << "Qualifiers and Flags:\n"
                  << "  -C           List Customized devices currently configured in the system (Default).\n"
                  << "  -P           List Predefined device categories and system supported classes.\n"
                  << "  -c Class     Displays devices belonging to the specified device class.\n"
                  << "               (e.g., DiskDrive, Net, Display, Processor, System).\n"
                  << "  -s Subclass  Displays devices connected via the specified subclass or bus\n"
                  << "               (e.g., PCI, USB, ROOT, SCSI).\n"
                  << "  -t Type      Displays devices matching the specified device type.\n"
                  << "  -l Name      Displays information only for the device with the logical name.\n"
                  << "               (e.g., hdisk0, ent0, proc0, sys0).\n"
                  << "  -S State     Filters by device state (Available, Defined, Stopped).\n"
                  << "  -H           Prints the column headers before displaying device records.\n"
                  << "  -F Format    User-defined colon or custom token format. Supported tokens:\n"
                  << "               name, status/state, location, description, class, subclass, type.\n"
                  << "               Example: -F \"name:status:location:description\"\n"
                  << "  -r Column    Displays all valid, distinct values for the specified column:\n"
                  << "               (class, subclass, state, type).\n"
                  << "  -h, --help   Displays this help manual.\n";
    }

    void Render(const std::vector<Device>& devices) const override {
        if (!m_options.customFormat.empty()) {
            RenderFormatted(devices, m_options.customFormat);
            return;
        }
        if (!m_options.listColumn.empty()) {
            RenderColumnValues(devices, m_options.listColumn);
            return;
        }
        if (m_options.predefinedMode) {
            RenderPredefinedDevices(devices);
            return;
        }
        RenderDevices(devices);
    }

private:
    void RenderDevices(const std::vector<Device>& devices) const {

        constexpr int nameWidth = 12;
        constexpr int statusWidth = 12;
        constexpr int locationWidth = 28;

        if (m_options.showHeaders) {
            std::cout << std::left 
                      << std::setw(nameWidth) << "NAME" << " | "
                      << std::setw(statusWidth) << "STATUS" << " | "
                      << std::setw(locationWidth) << "LOCATION" << " | "
                      << "DESCRIPTION\n"
                      << std::string(nameWidth, '-') << "-+-"
                      << std::string(statusWidth, '-') << "-+-"
                      << std::string(locationWidth, '-') << "-+-"
                      << std::string(32, '-') << "\n";
        }

        for (const auto& dev : devices) {
            std::cout << std::left 
                      << std::setw(nameWidth) << FitColumn(dev.GetName(), nameWidth) << " | "
                      << std::setw(statusWidth) << FitColumn(dev.GetState(), statusWidth) << " | "
                      << std::setw(locationWidth) << FitColumn(dev.GetLocation(), locationWidth) << " | "
                      << DisplayValue(dev.GetDescription()) << "\n";
        }
    }

    void RenderPredefinedDevices(const std::vector<Device>& devices) const {
        constexpr int classWidth = 18;
        constexpr int subclassWidth = 16;
        constexpr int typeWidth = 20;

        if (m_options.showHeaders) {
            std::cout << std::left 
                      << std::setw(classWidth) << "CLASS" << " | "
                      << std::setw(subclassWidth) << "SUBCLASS" << " | "
                      << std::setw(typeWidth) << "TYPE" << " | "
                      << "DESCRIPTION\n"
                      << std::string(classWidth, '-') << "-+-"
                      << std::string(subclassWidth, '-') << "-+-"
                      << std::string(typeWidth, '-') << "-+-"
                      << std::string(32, '-') << "\n";
        }

        std::set<std::string> seen;
        for (const auto& dev : devices) {
            std::string key = dev.GetClass() + "|" + dev.GetSubClass() + "|" + dev.GetType();
            if (seen.insert(key).second) {
                std::cout << std::left 
                          << std::setw(classWidth) << FitColumn(dev.GetClass(), classWidth) << " | "
                          << std::setw(subclassWidth) << FitColumn(dev.GetSubClass(), subclassWidth) << " | "
                          << std::setw(typeWidth) << FitColumn(dev.GetType(), typeWidth) << " | "
                          << DisplayValue(dev.GetDescription()) << "\n";
            }
        }
    }

    void RenderColumnValues(const std::vector<Device>& devices, const std::string& column) const {
        std::set<std::string> values;
        for (const auto& dev : devices) {
            std::string val = dev.GetField(column);
            if (!val.empty()) {
                values.insert(val);
            }
        }
        for (const auto& val : values) {
            std::cout << val << "\n";
        }
    }

    static std::string DisplayValue(const std::string& value) {
        std::string display;
        display.reserve(value.size());
        bool pendingSpace = false;

        for (unsigned char ch : value) {
            if (std::isspace(ch)) {
                pendingSpace = !display.empty();
            } else {
                if (pendingSpace) display.push_back(' ');
                display.push_back(static_cast<char>(ch));
                pendingSpace = false;
            }
        }

        return display.empty() ? "-" : display;
    }

    static std::string FitColumn(const std::string& value, size_t width) {
        const std::string display = DisplayValue(value);
        if (display.size() <= width) return display;
        if (width <= 3) return display.substr(0, width);
        return display.substr(0, width - 3) + "...";
    }

    void RenderFormatted(const std::vector<Device>& devices, const std::string& format) const {
        auto tokens = StringUtil::Split(format, ':');
        for (const auto& dev : devices) {
            for (size_t i = 0; i < tokens.size(); ++i) {
                std::cout << dev.GetField(tokens[i]);
                if (i + 1 < tokens.size()) std::cout << ":";
            }
            std::cout << "\n";
        }
    }

    const CommandLineOptions& m_options;
};

// ============================================================================
// Application Controller (Core Engine)
// ============================================================================
class LsDevApp {
public:
    LsDevApp(const CommandLineOptions& options,
             IDeviceEnumerator& enumerator,
             const DeviceFilter& filter,
             const IDeviceRenderer& renderer)
        : m_options(options),
          m_enumerator(enumerator),
          m_filter(filter),
          m_renderer(renderer) {}

    int Run() {
        if (m_options.showHelp) {
            m_renderer.ShowHelp();
            return 0;
        }

        auto allDevices = m_enumerator.EnumerateDevices(m_filter.IncludesNonPresentDevices());
        auto filtered = m_filter.Apply(allDevices);

        m_renderer.Render(filtered);

        return 0;
    }

private:
    const CommandLineOptions& m_options;
    IDeviceEnumerator& m_enumerator;
    const DeviceFilter& m_filter;
    const IDeviceRenderer& m_renderer;
};

// ============================================================================
// Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    try {
        ArgumentParser parser;
        CommandLineOptions opts = parser.Parse(argc, argv);
        SetupApiDeviceEnumerator enumerator;
        DeviceFilter filter(opts);
        ConsoleDeviceRenderer renderer(opts);
        LsDevApp app(opts, enumerator, filter, renderer);
        return app.Run();
    } catch (const std::exception& ex) {
        std::cerr << "lsdev: fatal error: " << ex.what() << "\n";
        return 1;
    }
}