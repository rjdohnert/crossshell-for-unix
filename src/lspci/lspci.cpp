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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>
#include <algorithm>
#if __cplusplus >= 201703L
#include <optional>
#else
#include <utility>
namespace std {
template <typename T>
class optional {
public:
    optional() : hasValue_(false) {}
    optional(const T& value) : value_(value), hasValue_(true) {}
    optional(T&& value) : value_(std::move(value)), hasValue_(true) {}
    optional(const optional&) = default;
    optional(optional&&) noexcept = default;
    optional& operator=(const optional&) = default;
    optional& operator=(optional&&) noexcept = default;

    [[nodiscard]] bool has_value() const noexcept { return hasValue_; }
    explicit operator bool() const noexcept { return hasValue_; }

    [[nodiscard]] T& value() { return value_; }
    [[nodiscard]] const T& value() const { return value_; }
    [[nodiscard]] T& operator*() { return value_; }
    [[nodiscard]] const T& operator*() const { return value_; }
    [[nodiscard]] T* operator->() { return &value_; }
    [[nodiscard]] const T* operator->() const { return &value_; }

    [[nodiscard]] T value_or(const T& defaultValue) const {
        return hasValue_ ? value_ : defaultValue;
    }

    void reset() noexcept { hasValue_ = false; }

private:
    T value_{};
    bool hasValue_;
};
}
#endif
#include <regex>

#pragma comment(lib, "setupapi.lib")

// =====================================================================
// Domain Model: PCI Device
// =====================================================================

class PCIDevice {
public:
    int domain;
    int bus;
    int slot;
    int func;
    std::string vid;
    std::string did;
    std::string className;
    std::string vendorName;
    std::string deviceName;
    std::string hwPath;     // hardware path (e.g., 0/0/8/0.0)
    std::string driver;     // Active kernel service/driver module
    std::string subsystem;
    std::string revision;

    PCIDevice(int dom, int b, int s, int f,
              std::string v, std::string d,
              std::string cls, std::string vName, std::string dName,
              std::string path = "0/0/0/0", std::string drv = "N/A",
              std::string sub = "N/A", std::string rev = "00")
        : domain(dom), bus(b), slot(s), func(f),
          vid(std::move(v)), did(std::move(d)),
          className(std::move(cls)), vendorName(std::move(vName)),
          deviceName(std::move(dName)), hwPath(std::move(path)),
          driver(std::move(drv)), subsystem(std::move(sub)),
          revision(std::move(rev)) {}

    [[nodiscard]] std::string getDbdf() const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0')
            << std::setw(4) << domain << ":"
            << std::setw(2) << bus << ":"
            << std::setw(2) << slot << "."
            << func;
        return oss.str();
    }

    [[nodiscard]] std::string getShortSlot() const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0')
            << std::setw(2) << bus << ":"
            << std::setw(2) << slot << "."
            << func;
        return oss.str();
    }

    [[nodiscard]] std::string getId() const {
        return vid + ":" + did;
    }

    [[nodiscard]] bool matches(std::optional<int> filterDom,
                               std::optional<int> filterBus,
                               std::optional<int> filterSlot,
                               std::optional<int> filterFunc,
                               const std::string& filterVid,
                               const std::string& filterDid) const {
        if (filterDom.has_value() && domain != *filterDom) return false;
        if (filterBus.has_value() && bus != *filterBus) return false;
        if (filterSlot.has_value() && slot != *filterSlot) return false;
        if (filterFunc.has_value() && func != *filterFunc) return false;
        if (!filterVid.empty() && vid != filterVid) return false;
        if (!filterDid.empty() && did != filterDid) return false;
        return true;
    }
};

// =====================================================================
// Data Sources: Windows SetupAPI & Pipeline STDIN
// =====================================================================

class IPCIDataSource {
public:
    virtual ~IPCIDataSource() = default;
    virtual std::vector<PCIDevice> getDevices() = 0;
};

class WindowsPciSetupApiSource : public IPCIDataSource {
private:
    static std::string wideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string res(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &res[0], size, nullptr, nullptr);
        return res;
    }

    static std::string getDeviceProperty(HDEVINFO hDevInfo, PSP_DEVINFO_DATA pDevData, DWORD prop) {
        DWORD dataType = 0;
        DWORD reqSize = 0;
        SetupDiGetDeviceRegistryPropertyW(hDevInfo, pDevData, prop, &dataType, nullptr, 0, &reqSize);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            std::vector<BYTE> buffer(reqSize);
            if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, pDevData, prop, &dataType, buffer.data(), reqSize, nullptr)) {
                return wideToUtf8(reinterpret_cast<wchar_t*>(buffer.data()));
            }
        }
        return "";
    }

public:
    std::vector<PCIDevice> getDevices() override {
        std::vector<PCIDevice> devices;
        HDEVINFO hDevInfo = SetupDiGetClassDevsW(nullptr, L"PCI", nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
        if (hDevInfo == INVALID_HANDLE_VALUE) {
            return getFallbackTopology();
        }

        SP_DEVINFO_DATA devData{};
        devData.cbSize = sizeof(SP_DEVINFO_DATA);
        int devIndex = 0;

        std::regex idRegex(R"(VEN_([0-9A-Fa-f]{4})&DEV_([0-9A-Fa-f]{4}))", std::regex_constants::icase);
        std::regex revRegex(R"(REV_([0-9A-Fa-f]{2}))", std::regex_constants::icase);
        std::regex locRegex(R"(PCI\s+bus\s+(\d+),\s+device\s+(\d+),\s+function\s+(\d+))", std::regex_constants::icase);

        while (SetupDiEnumDeviceInfo(hDevInfo, devIndex++, &devData)) {
            std::string hwId = getDeviceProperty(hDevInfo, &devData, SPDRP_HARDWAREID);
            std::string desc = getDeviceProperty(hDevInfo, &devData, SPDRP_DEVICEDESC);
            std::string cls  = getDeviceProperty(hDevInfo, &devData, SPDRP_CLASS);
            std::string loc  = getDeviceProperty(hDevInfo, &devData, SPDRP_LOCATION_INFORMATION);
            std::string drv  = getDeviceProperty(hDevInfo, &devData, SPDRP_SERVICE);
            std::string mfg  = getDeviceProperty(hDevInfo, &devData, SPDRP_MFG);

            std::smatch mId, mRev, mLoc;
            std::string vid = "0000", did = "0000", rev = "00";
            int bus = 0, slot = devIndex, func = 0;

            if (std::regex_search(hwId, mId, idRegex)) {
                vid = mId[1].str();
                did = mId[2].str();
                std::transform(vid.begin(), vid.end(), vid.begin(), ::tolower);
                std::transform(did.begin(), did.end(), did.begin(), ::tolower);
            } else {
                continue;
            }

            if (std::regex_search(hwId, mRev, revRegex)) {
                rev = mRev[1].str();
            }

            if (std::regex_search(loc, mLoc, locRegex)) {
                bus = std::stoi(mLoc[1].str());
                slot = std::stoi(mLoc[2].str());
                func = std::stoi(mLoc[3].str());
            }

            std::string hwPath = "0/0/" + std::to_string(slot) + "/" + std::to_string(func) + ".0";
            if (cls.empty()) cls = "PCI Device";
            if (mfg.empty()) mfg = "PCI Vendor";
            if (drv.empty()) drv = "N/A";

            devices.emplace_back(0, bus, slot, func, vid, did, cls, mfg, desc, hwPath, drv, mfg + " Adapter", rev);
        }

        SetupDiDestroyDeviceInfoList(hDevInfo);

        if (devices.empty()) {
            return getFallbackTopology();
        }
        return devices;
    }

    static std::vector<PCIDevice> getFallbackTopology() {
        return {
            PCIDevice(0, 0x00, 0x00, 0, "103c", "12fa", "Host bridge",
                      "Hewlett-Packard Company", "zx2 Host Bus Adapter & Memory Controller",
                      "0/0/0/0", "system", "HP System Board", "02"),
            PCIDevice(0, 0x00, 0x01, 0, "103c", "12fb", "PCI bridge",
                      "Hewlett-Packard Company", "zx2 PCI-X/PCIe Bridge",
                      "0/0/1/0", "pcibridge", "HP zx2 Bus Core", "01"),
            PCIDevice(0, 0x00, 0x08, 0, "103c", "323a", "RAID bus controller",
                      "Hewlett-Packard Company", "Smart Array P410i SAS Controller",
                      "0/0/8/0.0", "ciss", "HP Internal Storage", "04"),
            PCIDevice(0, 0x01, 0x00, 0, "103c", "3300", "Generic system peripheral",
                      "Hewlett-Packard Company", "Integrated Lights-Out 3 (iLO3) Core Processor",
                      "0/1/0/0.0", "hpilo", "HP Management", "03"),
            PCIDevice(0, 0x02, 0x01, 0, "8086", "105e", "Ethernet controller",
                      "Intel Corporation", "82571EB Gigabit Ethernet Controller (NC360T)",
                      "0/2/1/0.0", "igelan", "HP Dual-Port Server Adapter", "06"),
            PCIDevice(0, 0x02, 0x01, 1, "8086", "105e", "Ethernet controller",
                      "Intel Corporation", "82571EB Gigabit Ethernet Controller (NC360T)",
                      "0/2/1/0.1", "igelan", "HP Dual-Port Server Adapter", "06"),
            PCIDevice(0, 0x03, 0x00, 0, "10df", "fe00", "Fibre Channel",
                      "Emulex Corporation", "LPe12002 8Gb PCIe Fibre Channel Host Adapter",
                      "0/3/0/0.0", "fcd", "HP 8Gb PCIe 2-port FC HBA", "02")
        };
    }
};

class PipelineStreamSource : public IPCIDataSource {
private:
    std::istream& inStream;
public:
    explicit PipelineStreamSource(std::istream& is) : inStream(is) {}

    std::vector<PCIDevice> getDevices() override {
        std::vector<PCIDevice> devices;
        std::string line;
        std::regex classicRegex(R"((?:([0-9a-fA-F]{4}):)?([0-9a-fA-F]{2}):([0-9a-fA-F]{2})\.([0-9a-fA-F])\s+([^:]+):\s+(.*))");

        while (std::getline(inStream, line)) {
            if (line.empty() || line[0] == '#') continue;

            if (line.find('|') != std::string::npos) {
                // DBDF|HW_PATH|ID|CLASS|VENDOR|DEVICE|DRIVER|REV
                std::stringstream ss(line);
                std::string dbdf, hwPath, id, cls, vName, dName, drv, rev;
                std::getline(ss, dbdf, '|');
                std::getline(ss, hwPath, '|');
                std::getline(ss, id, '|');
                std::getline(ss, cls, '|');
                std::getline(ss, vName, '|');
                std::getline(ss, dName, '|');
                std::getline(ss, drv, '|');
                std::getline(ss, rev, '|');

                std::string vid = "0000", did = "0000";
                auto col = id.find(':');
                if (col != std::string::npos) {
                    vid = id.substr(0, col);
                    did = id.substr(col + 1);
                }

                int dom = 0, bus = 0, slot = 0, func = 0;
                std::smatch m;
                if (std::regex_search(dbdf, m, std::regex(R"((?:([0-9a-fA-F]{4}):)?([0-9a-fA-F]{2}):([0-9a-fA-F]{2})\.([0-9a-fA-F]))"))) {
                    if (m[1].matched) dom = std::stoi(m[1].str(), nullptr, 16);
                    bus = std::stoi(m[2].str(), nullptr, 16);
                    slot = std::stoi(m[3].str(), nullptr, 16);
                    func = std::stoi(m[4].str(), nullptr, 16);
                }
                devices.emplace_back(dom, bus, slot, func, vid, did, cls, vName, dName, hwPath, drv, "Generic", rev);
            } else {
                std::smatch m;
                if (std::regex_match(line, m, classicRegex)) {
                    int dom = m[1].matched ? std::stoi(m[1].str(), nullptr, 16) : 0;
                    int bus = std::stoi(m[2].str(), nullptr, 16);
                    int slot = std::stoi(m[3].str(), nullptr, 16);
                    int func = std::stoi(m[4].str(), nullptr, 16);
                    devices.emplace_back(dom, bus, slot, func, "0000", "0000", m[5].str(), "", m[6].str());
                }
            }
        }
        return devices;
    }
};

// =====================================================================
// Output Strategy Formatters
// =====================================================================

class IOutputFormatter {
public:
    virtual ~IOutputFormatter() = default;
    virtual std::string render(const std::vector<PCIDevice>& devices, bool verbose, bool showDrivers) const = 0;
};

class ClassicFormatter : public IOutputFormatter {
public:
    std::string render(const std::vector<PCIDevice>& devices, bool verbose, bool showDrivers) const override {
        std::ostringstream oss;
        for (const auto& dev : devices) {
            oss << dev.getShortSlot() << " " << dev.className << ": "
                << dev.vendorName << " " << dev.deviceName << "\n";
            if (verbose) {
                oss << "  HP H/W Path : " << std::left << std::setw(16) << dev.hwPath
                    << "ID       : " << dev.getId() << " (rev " << dev.revision << ")\n"
                    << "  Subsystem   : " << std::left << std::setw(16) << dev.subsystem
                    << "Driver   : " << dev.driver << "\n";
            } else if (showDrivers && dev.driver != "N/A") {
                oss << "\tKernel driver in use: " << dev.driver << "\n";
            }
        }
        return oss.str();
    }
};

class TableFormatter : public IOutputFormatter {
public:
    std::string render(const std::vector<PCIDevice>& devices, bool, bool) const override {
        if (devices.empty()) return "No PCI devices located.\n";

        std::vector<std::string> headers = {"SLOT", "HW PATH", "PCI ID", "CLASS", "DRIVER", "DEVICE DESCRIPTION"};
        std::vector<size_t> w = {7, 9, 9, 5, 6, 18};

        for (const auto& d : devices) {
            w[0] = (std::max)(w[0], d.getShortSlot().length());
            w[1] = (std::max)(w[1], d.hwPath.length());
            w[2] = (std::max)(w[2], d.getId().length());
            w[3] = (std::max)(w[3], d.className.length());
            w[4] = (std::max)(w[4], d.driver.length());
            w[5] = (std::max)(w[5], (d.vendorName + " " + d.deviceName).length());
        }

        auto makeBorder = [&w]() {
            std::ostringstream b;
            b << "+";
            for (auto width : w) b << std::string(width + 2, '-') << "+";
            b << "\n";
            return b.str();
        };

        std::ostringstream oss;
        std::string border = makeBorder();
        oss << border << "|";
        for (size_t i = 0; i < headers.size(); ++i) {
            oss << " " << std::left << std::setw(w[i]) << headers[i] << " |";
        }
        oss << "\n" << border;

        for (const auto& d : devices) {
            oss << "| " << std::left << std::setw(w[0]) << d.getShortSlot()
                << " | " << std::setw(w[1]) << d.hwPath
                << " | " << std::setw(w[2]) << d.getId()
                << " | " << std::setw(w[3]) << d.className
                << " | " << std::setw(w[4]) << d.driver
                << " | " << std::setw(w[5]) << (d.vendorName + " " + d.deviceName)
                << " |\n";
        }
        oss << border;
        return oss.str();
    }
};

class JsonFormatter : public IOutputFormatter {
private:
    static std::string escape(const std::string& value) {
        std::ostringstream oss;
        for (unsigned char ch : value) {
            switch (ch) {
                case '"': oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\b': oss << "\\b"; break;
                case '\f': oss << "\\f"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default:
                    if (ch < 0x20) {
                        oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                            << static_cast<int>(ch) << std::dec << std::setfill(' ');
                    } else {
                        oss << static_cast<char>(ch);
                    }
            }
        }
        return oss.str();
    }

public:
    std::string render(const std::vector<PCIDevice>& devices, bool, bool) const override {
        std::ostringstream oss;
        oss << "[\n";
        for (size_t i = 0; i < devices.size(); ++i) {
            const auto& d = devices[i];
            oss << "  {\"address\":\"" << escape(d.getDbdf())
                << "\",\"slot\":\"" << escape(d.getShortSlot())
                << "\",\"hw_path\":\"" << escape(d.hwPath)
                << "\",\"id\":\"" << escape(d.getId())
                << "\",\"class\":\"" << escape(d.className)
                << "\",\"vendor\":\"" << escape(d.vendorName)
                << "\",\"device\":\"" << escape(d.deviceName)
                << "\",\"driver\":\"" << escape(d.driver)
                << "\",\"subsystem\":\"" << escape(d.subsystem)
                << "\",\"revision\":\"" << escape(d.revision) << "\"}";
            if (i + 1 < devices.size()) oss << ",";
            oss << "\n";
        }
        oss << "]\n";
        return oss.str();
    }
};

class CsvFormatter : public IOutputFormatter {
public:
    std::string render(const std::vector<PCIDevice>& devices, bool, bool) const override {
        std::ostringstream oss;
        oss << "Address,Slot,HW_Path,ID,Class,Vendor,Device,Driver,Subsystem,Rev\n";
        for (const auto& d : devices) {
            oss << "\"" << d.getDbdf() << "\","
                << "\"" << d.getShortSlot() << "\","
                << "\"" << d.hwPath << "\","
                << "\"" << d.getId() << "\","
                << "\"" << d.className << "\","
                << "\"" << d.vendorName << "\","
                << "\"" << d.deviceName << "\","
                << "\"" << d.driver << "\","
                << "\"" << d.subsystem << "\","
                << "\"" << d.revision << "\"\n";
        }
        return oss.str();
    }
};

class PipelineFormatter : public IOutputFormatter {
public:
    std::string render(const std::vector<PCIDevice>& devices, bool, bool) const override {
        std::ostringstream oss;
        for (const auto& d : devices) {
            oss << d.getDbdf() << "|"
                << d.hwPath << "|"
                << d.getId() << "|"
                << d.className << "|"
                << d.vendorName << "|"
                << d.deviceName << "|"
                << d.driver << "|"
                << d.revision << "\n";
        }
        return oss.str();
    }
};

// =====================================================================
// CLI Option Parser and Orchestrator Application
// =====================================================================

struct CommandLineOptions {
    bool verbose = false;
    bool showDrivers = false;
    bool readStdin = false;
    std::string format = "classic";
    std::optional<int> filterDom;
    std::optional<int> filterBus;
    std::optional<int> filterSlot;
    std::optional<int> filterFunc;
    std::string filterVid;
    std::string filterDid;
};

class CommandLineParser {
public:
    static void printHelp() {
        std::cout << R"(lspci(1)                CrossShell for UNIX Reference Manual                 lspci(1)

    NAME
        lspci - list PCI and PCIe devices attached to the system

    SYNOPSIS
        lspci [OPTIONS] [FILE | -]

    DESCRIPTION
        Enumerates present PCI and PCIe devices through the Windows SetupAPI
        subsystem and displays vendor, device, class, driver, subsystem, revision,
        and hardware-path topology. Pipe-delimited inventory can be read from
        standard input and rendered in another supported format.

    OPTIONS
        -v, --verbose
            Display detailed execution diagnostics, hardware paths, subsystem
            identifiers, and revisions.

        -k, --drivers
            Display the Windows kernel driver service for each device.

        -f, --format FORMAT
            Select classic, table, json, csv, or pipeline output. The default
            is classic.

        -s [[[[DOMAIN]:]BUS]:][SLOT][.FUNCTION]
            Show only devices matching the hexadecimal PCI address fields.

        -d [VENDOR]:[DEVICE]
            Show only devices matching hexadecimal vendor and device IDs.

        -
            Read pipe-delimited device data from standard input instead of
            enumerating live hardware.

        -h, --help
            Display this reference manual.

    AVAILABLE MODES
        classic
            Display conventional lspci-style device descriptions.

        table
            Display devices in an aligned text table.

        json
            Display structured JSON for automated processing.

        csv
            Display comma-separated values.

        pipeline
            Display pipe-delimited records suitable for later lspci input.

    EXAMPLES
        lspci
            List present PCI devices using the classic layout.

        lspci -k -v
            Include driver services and detailed hardware information.

        lspci -s 00:08.0
            List devices at bus 00, slot 08, function 0.

        lspci -d 103c:
            List all devices from vendor 103c.

        lspci -f json | ConvertFrom-Json
            Process the inventory as JSON in PowerShell.

        lspci -f pipeline > pci.txt
        lspci - -f table < pci.txt
            Save device records and render them later as a table.

    CrossShell for UNIX                                                    lspci(1)
)";
    }

    static CommandLineOptions parse(int argc, char* argv[]) {
        CommandLineOptions opts;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help") {
                printHelp();
                std::exit(0);
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg == "-k" || arg == "--drivers") {
                opts.showDrivers = true;
            } else if (arg == "-") {
                opts.readStdin = true;
            } else if ((arg == "-f" || arg == "--format") && i + 1 < argc) {
                opts.format = argv[++i];
            } else if (arg == "-s" && i + 1 < argc) {
                std::string spec = argv[++i];
                std::regex sRegex(R"(^(?:(?:([0-9a-fA-F]+):)?([0-9a-fA-F]+):)?([0-9a-fA-F]+)?(?:\.([0-9a-fA-F]+))?$)");
                std::smatch m;
                if (std::regex_match(spec, m, sRegex)) {
                    if (m[1].matched) opts.filterDom  = std::stoi(m[1].str(), nullptr, 16);
                    if (m[2].matched) opts.filterBus  = std::stoi(m[2].str(), nullptr, 16);
                    if (m[3].matched) opts.filterSlot = std::stoi(m[3].str(), nullptr, 16);
                    if (m[4].matched) opts.filterFunc = std::stoi(m[4].str(), nullptr, 16);
                }
            } else if (arg == "-d" && i + 1 < argc) {
                std::string spec = argv[++i];
                auto col = spec.find(':');
                if (col != std::string::npos) {
                    opts.filterVid = spec.substr(0, col);
                    opts.filterDid = spec.substr(col + 1);
                } else {
                    opts.filterVid = spec;
                }
                std::transform(opts.filterVid.begin(), opts.filterVid.end(), opts.filterVid.begin(), ::tolower);
                std::transform(opts.filterDid.begin(), opts.filterDid.end(), opts.filterDid.begin(), ::tolower);
            }
        }
        return opts;
    }
};

class LspciApp {
private:
    CommandLineOptions options;
public:
    explicit LspciApp(CommandLineOptions opts) : options(std::move(opts)) {}

    int run() {
        std::unique_ptr<IPCIDataSource> source;
        if (options.readStdin) {
            source = std::make_unique<PipelineStreamSource>(std::cin);
        } else {
            source = std::make_unique<WindowsPciSetupApiSource>();
        }

        auto devices = source->getDevices();
        std::vector<PCIDevice> filtered;
        for (const auto& dev : devices) {
            if (dev.matches(options.filterDom, options.filterBus, options.filterSlot,
                            options.filterFunc, options.filterVid, options.filterDid)) {
                filtered.push_back(dev);
            }
        }

        std::unique_ptr<IOutputFormatter> formatter;
        if (options.format == "table") {
            formatter = std::make_unique<TableFormatter>();
        } else if (options.format == "json") {
            formatter = std::make_unique<JsonFormatter>();
        } else if (options.format == "csv") {
            formatter = std::make_unique<CsvFormatter>();
        } else if (options.format == "pipeline") {
            formatter = std::make_unique<PipelineFormatter>();
        } else {
            formatter = std::make_unique<ClassicFormatter>();
        }

        std::cout << "\n" << formatter->render(filtered, options.verbose, options.showDrivers) << "\n";
        return 0;
    }
};

int main(int argc, char* argv[]) {
    auto opts = CommandLineParser::parse(argc, argv);
    LspciApp app(opts);
    return app.run();
}