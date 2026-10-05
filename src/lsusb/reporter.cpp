#include "reporter.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

std::string ClassicFormatter::render(const std::vector<USBDevice>& devices, bool verbose) const {
    std::ostringstream oss;
    for (const auto& dev : devices) {
        oss << "Bus " << std::setfill('0') << std::setw(3) << dev.bus
            << " Device " << std::setfill('0') << std::setw(3) << dev.device
            << ": ID " << dev.getId() << " " << dev.description << "\n";
        if (verbose) {
            oss << "  Device Class: " << std::left << std::setw(16) << dev.deviceClass
                << "Speed: " << dev.speed << "\n"
                << "  HP H/W Path : " << std::left << std::setw(16) << dev.hwPath
                << "S/N  : " << dev.serial << "\n";
        }
    }
    return oss.str();
}

std::string TableFormatter::render(const std::vector<USBDevice>& devices, bool) const {
    if (devices.empty()) return "No USB devices found.\n";

    std::vector<std::string> headers = {"BUS", "DEV", "ID", "CLASS", "SPEED", "HW PATH", "DESCRIPTION"};
    std::vector<size_t> w = {3, 3, 9, 5, 5, 7, 11};

    for (const auto& d : devices) {
        w[0] = (std::max)(w[0], std::to_string(d.bus).length());
        w[1] = (std::max)(w[1], std::to_string(d.device).length());
        w[2] = (std::max)(w[2], d.getId().length());
        w[3] = (std::max)(w[3], d.deviceClass.length());
        w[4] = (std::max)(w[4], d.speed.length());
        w[5] = (std::max)(w[5], d.hwPath.length());
        w[6] = (std::max)(w[6], d.description.length());
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
        oss << "| " << std::right << std::setw(w[0]) << std::setfill('0') << d.bus << " | "
            << std::right << std::setw(w[1]) << std::setfill('0') << d.device << " | "
            << std::left << std::setfill(' ')
            << std::setw(w[2]) << d.getId() << " | "
            << std::setw(w[3]) << d.deviceClass << " | "
            << std::setw(w[4]) << d.speed << " | "
            << std::setw(w[5]) << d.hwPath << " | "
            << std::setw(w[6]) << d.description << " |\n";
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

std::string JsonFormatter::render(const std::vector<USBDevice>& devices, bool) const {
    std::ostringstream oss;
    oss << "[\n";
    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& d = devices[i];
        oss << "  {\n"
            << "    \"bus\": " << d.bus << ",\n"
            << "    \"device\": " << d.device << ",\n"
            << "    \"id\": \"" << d.getId() << "\",\n"
            << "    \"vendor_id\": \"" << d.vid << "\",\n"
            << "    \"product_id\": \"" << d.pid << "\",\n"
            << "    \"class\": \"" << escapeJson(d.deviceClass) << "\",\n"
            << "    \"speed\": \"" << escapeJson(d.speed) << "\",\n"
            << "    \"hw_path\": \"" << escapeJson(d.hwPath) << "\",\n"
            << "    \"serial\": \"" << escapeJson(d.serial) << "\",\n"
            << "    \"description\": \"" << escapeJson(d.description) << "\"\n"
            << "  }" << (i + 1 < devices.size() ? "," : "") << "\n";
    }
    oss << "]\n";
    return oss.str();
}

std::string CsvFormatter::render(const std::vector<USBDevice>& devices, bool) const {
    std::ostringstream oss;
    oss << "Bus,Device,ID,Class,Speed,HW_Path,Serial,Description\n";
    for (const auto& d : devices) {
        oss << d.bus << ","
            << d.device << ","
            << "\"" << d.getId() << "\","
            << "\"" << d.deviceClass << "\","
            << "\"" << d.speed << "\","
            << "\"" << d.hwPath << "\","
            << "\"" << d.serial << "\","
            << "\"" << d.description << "\"\n";
    }
    return oss.str();
}

std::string PipelineFormatter::render(const std::vector<USBDevice>& devices, bool) const {
    std::ostringstream oss;
    for (const auto& d : devices) {
        oss << d.bus << "|"
            << d.device << "|"
            << d.getId() << "|"
            << d.deviceClass << "|"
            << d.speed << "|"
            << d.hwPath << "|"
            << d.description << "|"
            << d.serial << "\n";
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
