#include "reporter.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

std::string ClassicFormatter::render(const std::vector<BluetoothDevice>& devices, bool verbose, bool showDrivers) const {
    std::ostringstream oss;
    for (const auto& dev : devices) {
        oss << dev.getShortSlot() << " " << dev.deviceClass << ": "
            << dev.name << " [" << dev.macAddress << "]"
            << (dev.isConnected ? " (connected)" : "") << "\n";

        if (verbose) {
            oss << "  HP H/W Path : " << std::left << std::setw(18) << dev.hwPath
                << "Pseudo ID  : " << dev.getPseudoId() << "\n"
                << "  Class (CoD) : " << std::left << std::setw(18) << dev.codHex
                << "Status     : " << (dev.isConnected ? "Paired, Connected" : "Paired") << "\n"
                << "  Driver/Svc  : " << std::left << std::setw(18) << dev.driver
                << "Last Seen  : " << dev.lastSeen << "\n";
        } else if (showDrivers && dev.driver != "N/A") {
            oss << "\tKernel driver/service in use: " << dev.driver << "\n";
        }
    }
    return oss.str();
}

std::string TableFormatter::render(const std::vector<BluetoothDevice>& devices, bool, bool) const {
    if (devices.empty()) return "No paired Bluetooth devices located.\n";

    std::vector<std::string> headers = {"SLOT", "HW PATH", "BT ADDRESS", "CLASS", "DRIVER", "STATUS", "DEVICE NAME"};
    std::vector<size_t> w = {7, 9, 17, 5, 6, 9, 11};

    for (const auto& d : devices) {
        w[0] = (std::max)(w[0], d.getShortSlot().length());
        w[1] = (std::max)(w[1], d.hwPath.length());
        w[2] = (std::max)(w[2], d.macAddress.length());
        w[3] = (std::max)(w[3], d.deviceClass.length());
        w[4] = (std::max)(w[4], d.driver.length());
        w[5] = (std::max)(w[5], std::string(d.isConnected ? "Connected" : "Paired").length());
        w[6] = (std::max)(w[6], d.name.length());
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
        oss << "| " << std::left << std::setw(w[0]) << d.getShortSlot() << " | "
            << std::setw(w[1]) << d.hwPath << " | "
            << std::setw(w[2]) << d.macAddress << " | "
            << std::setw(w[3]) << d.deviceClass << " | "
            << std::setw(w[4]) << d.driver << " | "
            << std::setw(w[5]) << (d.isConnected ? "Connected" : "Paired") << " | "
            << std::setw(w[6]) << d.name << " |\n";
    }
    oss << border;
    return oss.str();
}

static std::string escapeJson(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\b') o << "\\b";
        else if (c == '\f') o << "\\f";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else o << c;
    }
    return o.str();
}

std::string JsonFormatter::render(const std::vector<BluetoothDevice>& devices, bool, bool) const {
    std::ostringstream oss;
    oss << "[\n";
    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& d = devices[i];
        oss << "  {\n"
            << "    \"slot\": \"" << d.getShortSlot() << "\",\n"
            << "    \"hw_path\": \"" << escapeJson(d.hwPath) << "\",\n"
            << "    \"mac_address\": \"" << d.macAddress << "\",\n"
            << "    \"pseudo_id\": \"" << d.getPseudoId() << "\",\n"
            << "    \"name\": \"" << escapeJson(d.name) << "\",\n"
            << "    \"class\": \"" << escapeJson(d.deviceClass) << "\",\n"
            << "    \"class_of_device\": \"" << d.codHex << "\",\n"
            << "    \"driver\": \"" << escapeJson(d.driver) << "\",\n"
            << "    \"paired\": " << (d.isPaired ? "true" : "false") << ",\n"
            << "    \"connected\": " << (d.isConnected ? "true" : "false") << ",\n"
            << "    \"last_seen\": \"" << escapeJson(d.lastSeen) << "\"\n"
            << "  }" << (i + 1 < devices.size() ? "," : "") << "\n";
    }
    oss << "]\n";
    return oss.str();
}

std::string CsvFormatter::render(const std::vector<BluetoothDevice>& devices, bool, bool) const {
    std::ostringstream oss;
    oss << "Slot,HW_Path,MAC_Address,Class,Driver,Status,CoD,Device_Name\n";
    for (const auto& d : devices) {
        oss << "\"" << d.getShortSlot() << "\","
            << "\"" << d.hwPath << "\","
            << "\"" << d.macAddress << "\","
            << "\"" << d.deviceClass << "\","
            << "\"" << d.driver << "\","
            << "\"" << (d.isConnected ? "Connected" : "Paired") << "\","
            << "\"" << d.codHex << "\","
            << "\"" << d.name << "\"\n";
    }
    return oss.str();
}

std::string PipelineFormatter::render(const std::vector<BluetoothDevice>& devices, bool, bool) const {
    std::ostringstream oss;
    for (const auto& d : devices) {
        oss << d.getShortSlot() << "|"
            << d.hwPath << "|"
            << d.macAddress << "|"
            << d.deviceClass << "|"
            << d.driver << "|"
            << d.name << "|"
            << d.codHex << "|"
            << (d.isConnected ? "Connected" : "Paired") << "\n";
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
