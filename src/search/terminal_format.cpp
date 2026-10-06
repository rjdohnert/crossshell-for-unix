#include "terminal_format.hpp"

void ConsoleTerminal::EnableVirtualTerminal() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
}

std::string ConsoleTerminal::FormatTypeBadge(bool isDir, bool isSym) {
    if (isDir)      return Blue + "[DIR]" + Reset;
    if (isSym) return Magenta + "[SYM]" + Reset;
    return Green + "[FILE]" + Reset;
}

std::optional<uintmax_t> SizeFormatter::Parse(const std::string& str) {
    if (str.empty()) return std::nullopt;
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);

    double multiplier = 1.0;
    size_t numEnd = 0;
    while (numEnd < s.size() && (std::isdigit(static_cast<unsigned char>(s[numEnd])) || s[numEnd] == '.')) {
        numEnd++;
    }
    if (numEnd == 0) return std::nullopt;

    double val = 0.0;
    try {
        val = std::stod(s.substr(0, numEnd));
    } catch (...) {
        return std::nullopt;
    }

    std::string unit = s.substr(numEnd);
    if (unit.empty() || unit == "B" || unit == "BYTES") multiplier = 1.0;
    else if (unit == "K" || unit == "KB") multiplier = 1024.0;
    else if (unit == "M" || unit == "MB") multiplier = 1024.0 * 1024.0;
    else if (unit == "G" || unit == "GB") multiplier = 1024.0 * 1024.0 * 1024.0;
    else if (unit == "T" || unit == "TB") multiplier = 1024.0 * 1024.0 * 1024.0 * 1024.0;
    else return std::nullopt;

    return static_cast<uintmax_t>(val * multiplier);
}

std::string SizeFormatter::Format(uintmax_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int idx = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && idx < 4) {
        size /= 1024.0;
        idx++;
    }
    std::ostringstream ss;
    if (idx == 0) ss << bytes << " B";
    else ss << std::fixed << std::setprecision(1) << size << " " << units[idx];
    return ss.str();
}

std::chrono::system_clock::time_point DateTimeFormatter::ToSystemTime(fs::file_time_type ftime) {
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
    );
}

std::optional<std::chrono::system_clock::time_point> DateTimeFormatter::Parse(const std::string& str) {
    if (str.empty()) return std::nullopt;
    auto now = std::chrono::system_clock::now();

    char lastChar = static_cast<char>(std::tolower(static_cast<unsigned char>(str.back())));
    if (lastChar == 'd' || lastChar == 'h' || lastChar == 'm' || lastChar == 's') {
        try {
            long long val = std::stoll(str.substr(0, str.size() - 1));
            if (lastChar == 'd') return now - std::chrono::hours(val * 24);
            if (lastChar == 'h') return now - std::chrono::hours(val);
            if (lastChar == 'm') return now - std::chrono::minutes(val);
            if (lastChar == 's') return now - std::chrono::seconds(val);
        } catch (...) {
            return std::nullopt;
        }
    }

    std::tm tm = {};
    std::istringstream ss(str);
    if (str.find(':') != std::string::npos) {
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    } else {
        ss >> std::get_time(&tm, "%Y-%m-%d");
    }

    if (ss.fail()) return std::nullopt;
    std::time_t tt = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(tt);
}

std::string DateTimeFormatter::Format(fs::file_time_type ftime) {
    auto sctp = ToSystemTime(ftime);
    std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
    std::tm* local_tm = std::localtime(&tt);
    char buf[32];
    if (local_tm) {
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", local_tm);
        return std::string(buf);
    }
    return "N/A";
}

std::string AttributeInspector::GetAttributesString(const fs::path& p, DWORD* rawAttr) {
    DWORD attr = GetFileAttributesW(p.c_str());
    if (rawAttr) *rawAttr = attr;
    if (attr == INVALID_FILE_ATTRIBUTES) return "-------";

    std::string s = "";
    s += (attr & FILE_ATTRIBUTE_READONLY) ? 'R' : '-';
    s += (attr & FILE_ATTRIBUTE_HIDDEN)   ? 'H' : '-';
    s += (attr & FILE_ATTRIBUTE_SYSTEM)   ? 'S' : '-';
    s += (attr & FILE_ATTRIBUTE_ARCHIVE)  ? 'A' : '-';
    s += (attr & FILE_ATTRIBUTE_COMPRESSED) ? 'C' : '-';
    s += (attr & FILE_ATTRIBUTE_ENCRYPTED)  ? 'E' : '-';
    return s;
}
