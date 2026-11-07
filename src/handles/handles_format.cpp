#include "handles_format.hpp"

#include <windows.h>

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>

namespace {
struct FormattedProcess {
    const ProcessHandleInfo* process;
    std::string name;
};

const char* StatusName(HandleCountStatus status) {
    switch (status) {
        case HandleCountStatus::available: return "available";
        case HandleCountStatus::accessDenied: return "access_denied";
        case HandleCountStatus::unavailable: return "unavailable";
    }
    return "unavailable";
}

bool ToUtf8(const std::wstring& value, std::string& output) {
    if (value.empty()) {
        output.clear();
        return true;
    }
    if (value.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
        return false;
    }
    const int inputLength = static_cast<int>(value.size());
    const int required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), inputLength,
        nullptr, 0, nullptr, nullptr);
    if (required <= 0) return false;
    output.resize(static_cast<size_t>(required));
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        inputLength, output.data(), required, nullptr, nullptr) == required;
}

std::string JsonString(const std::string& value) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string escaped;
    escaped.push_back('"');
    for (unsigned char character : value) {
        if (character == '"' || character == '\\') {
            escaped.push_back('\\');
            escaped.push_back(static_cast<char>(character));
        } else if (character < 0x20) {
            escaped += "\\u00";
            escaped.push_back(hex[(character >> 4) & 0x0f]);
            escaped.push_back(hex[character & 0x0f]);
        } else {
            escaped.push_back(static_cast<char>(character));
        }
    }
    escaped.push_back('"');
    return escaped;
}

std::string CsvString(const std::string& value) {
    std::string quoted;
    quoted.reserve(value.size() + 2);
    quoted.push_back('"');
    for (char character : value) {
        if (character == '"') quoted.push_back('"');
        quoted.push_back(character);
    }
    quoted.push_back('"');
    return quoted;
}

std::string CountText(const ProcessHandleInfo& process) {
    return process.status == HandleCountStatus::available
        ? std::to_string(process.handleCount)
        : "";
}

std::string TableCount(const ProcessHandleInfo& process) {
    if (process.status == HandleCountStatus::accessDenied) return "<AccessDenied>";
    if (process.status == HandleCountStatus::unavailable) return "<Unavailable>";
    return std::to_string(process.handleCount);
}
}

bool FormatProcesses(OutputFormat format,
                     const std::vector<ProcessHandleInfo>& processes,
                     std::string& output, std::wstring& error) {
    output.clear();
    error.clear();

    std::vector<FormattedProcess> rows;
    rows.reserve(processes.size());
    for (const ProcessHandleInfo& process : processes) {
        std::string name;
        if (!ToUtf8(process.name, name)) {
            error = L"cannot encode a process name as UTF-8";
            return false;
        }
        rows.push_back({&process, std::move(name)});
    }

    std::ostringstream stream;
    if (format == OutputFormat::json) {
        stream << "[\n";
        for (size_t i = 0; i < rows.size(); ++i) {
            const ProcessHandleInfo& process = *rows[i].process;
            stream << "  {\"pid\":" << process.pid << ",\"handleCount\":";
            if (process.status == HandleCountStatus::available) {
                stream << process.handleCount;
            } else {
                stream << "null";
            }
            stream << ",\"processName\":" << JsonString(rows[i].name)
                   << ",\"accessDenied\":"
                   << (process.status == HandleCountStatus::accessDenied
                           ? "true" : "false")
                   << ",\"status\":" << JsonString(StatusName(process.status))
                   << "}" << (i + 1 < rows.size() ? "," : "") << '\n';
        }
        stream << "]\n";
    } else if (format == OutputFormat::csv) {
        stream << "PID,HandleCount,ProcessName,AccessDenied,Status\n";
        for (const FormattedProcess& row : rows) {
            const ProcessHandleInfo& process = *row.process;
            stream << process.pid << ',' << CountText(process) << ','
                   << CsvString(row.name) << ','
                   << (process.status == HandleCountStatus::accessDenied
                           ? "true" : "false")
                   << ',' << StatusName(process.status) << '\n';
        }
    } else if (format == OutputFormat::tsv) {
        stream << "PID\tHandleCount\tProcessName\tStatus\n";
        for (const FormattedProcess& row : rows) {
            const ProcessHandleInfo& process = *row.process;
            stream << process.pid << '\t' << CountText(process) << '\t'
                   << row.name << '\t' << StatusName(process.status) << '\n';
        }
    } else if (format == OutputFormat::pipe) {
        stream << "PID|HandleCount|ProcessName|Status\n";
        for (const FormattedProcess& row : rows) {
            const ProcessHandleInfo& process = *row.process;
            stream << process.pid << '|' << CountText(process) << '|'
                   << row.name << '|' << StatusName(process.status) << '\n';
        }
    } else {
        size_t nameWidth = 11;
        for (const FormattedProcess& row : rows) {
            nameWidth = (std::max)(nameWidth, row.name.size());
        }
        stream << std::left << std::setw(10) << "PID"
               << std::setw(18) << "HandleCount"
               << std::setw(static_cast<int>(nameWidth)) << "ProcessName"
               << "Status\n";
        stream << std::string(28 + nameWidth + 6, '-') << '\n';
        for (const FormattedProcess& row : rows) {
            const ProcessHandleInfo& process = *row.process;
            stream << std::left << std::setw(10) << process.pid
                   << std::setw(18) << TableCount(process)
                   << std::setw(static_cast<int>(nameWidth)) << row.name
                   << StatusName(process.status) << '\n';
        }
    }

    output = stream.str();
    return true;
}
