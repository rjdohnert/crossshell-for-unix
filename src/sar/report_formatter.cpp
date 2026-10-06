#include "report_formatter.hpp"

std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_s(&tm_buf, &now_time);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%H:%M:%S");
    return oss.str();
}

std::string GetSystemDate() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_s(&tm_buf, &now_time);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%m/%d/%y");
    return oss.str();
}

std::string GetHostNameString() {
    char name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(name);
    if (GetComputerNameA(name, &size)) return std::string(name);
    return "WINDOWS-NT";
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return {};
    std::string result(size_needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size_needed, nullptr, nullptr);
    return result;
}

void PrintHeaderLine() {
    std::cout << "sar (" << GetHostNameString()
              << ")   " << GetSystemDate() << "\n\n";
}

std::string FormatPercent(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << value << "%";
    return oss.str();
}

std::string FormatKib(uint64_t kib) {
    const double kibAsDouble = static_cast<double>(kib);

    std::ostringstream oss;
    if (kib >= 1024ULL * 1024ULL) {
        oss << std::fixed << std::setprecision(1) << (kibAsDouble / (1024.0 * 1024.0)) << "G";
    } else if (kib >= 1024ULL) {
        oss << std::fixed << std::setprecision(1) << (kibAsDouble / 1024.0) << "M";
    } else {
        oss << kib << "K";
    }
    return oss.str();
}

std::string FormatRate(double value, const std::string& unit) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << value << unit;
    return oss.str();
}

std::string FormatWaitMs(double ms) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << ms << "ms";
    return oss.str();
}

void PrintSectionHeader(const std::string& title, const std::vector<std::string>& columns) {
    std::cout << title << "\n";
    std::cout << std::left;
    std::cout << std::setw(8) << "time";
    for (const auto& column : columns) {
        std::cout << " " << std::setw(12) << column;
    }
    std::cout << "\n";
}

void PrintCsvSectionHeader(const std::string& title, const std::vector<std::string>& columns) {
    std::cout << "# " << title << "\n";
    std::cout << "time";
    for (const auto& column : columns) {
        std::cout << "," << column;
    }
    std::cout << "\n";
}

void PrintJsonSectionHeader(const std::string& title) {
    std::cout << "[\n";
    std::cout << "  {\"section\":\"" << title << "\"}\n";
}

std::string JsonEscape(const std::string& value) {
    std::string out;
    for (char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            default: out += ch; break;
        }
    }
    return out;
}
