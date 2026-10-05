#include "reporter.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

std::string ClassicFormatter::render(const std::vector<PCIDevice>& devices, bool verbose, bool showDrivers) const {
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

std::string TableFormatter::render(const std::vector<PCIDevice>& devices, bool, bool) const {
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

static std::string jsonEscape(const std::string& value) {
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

std::string JsonFormatter::render(const std::vector<PCIDevice>& devices, bool, bool) const {
    std::ostringstream oss;
    oss << "[\n";
    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& d = devices[i];
        oss << "  {\"address\":\"" << jsonEscape(d.getDbdf())
            << "\",\"slot\":\"" << jsonEscape(d.getShortSlot())
            << "\",\"hw_path\":\"" << jsonEscape(d.hwPath)
            << "\",\"id\":\"" << jsonEscape(d.getId())
            << "\",\"class\":\"" << jsonEscape(d.className)
            << "\",\"vendor\":\"" << jsonEscape(d.vendorName)
            << "\",\"device\":\"" << jsonEscape(d.deviceName)
            << "\",\"driver\":\"" << jsonEscape(d.driver)
            << "\",\"subsystem\":\"" << jsonEscape(d.subsystem)
            << "\",\"revision\":\"" << jsonEscape(d.revision) << "\"}";
        if (i + 1 < devices.size()) oss << ",";
        oss << "\n";
    }
    oss << "]\n";
    return oss.str();
}

std::string CsvFormatter::render(const std::vector<PCIDevice>& devices, bool, bool) const {
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

std::string PipelineFormatter::render(const std::vector<PCIDevice>& devices, bool, bool) const {
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

std::unique_ptr<IOutputFormatter> FormatterFactory::create(const std::string& format) {
    if (format == "table") {
        return std::make_unique<TableFormatter>();
    } else if (format == "json") {
        return std::make_unique<JsonFormatter>();
    } else if (format == "csv") {
        return std::make_unique<CsvFormatter>();
    } else if (format == "pipeline") {
        return std::make_unique<PipelineFormatter>();
    } else {
        return std::make_unique<ClassicFormatter>();
    }
}
