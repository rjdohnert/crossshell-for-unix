#include "errpt_output.hpp"

#include <string>
#include <utility>

namespace {

bool IsFormulaWhitespace(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\r' ||
           value == '\n' || value == '\v' || value == '\f';
}

bool IsFormulaLike(std::string_view value) {
    std::size_t index = 0;
    while (index < value.size() &&
           IsFormulaWhitespace(static_cast<unsigned char>(value[index]))) {
        ++index;
    }
    if (index == value.size()) return false;
    const char first = value[index];
    return first == '=' || first == '+' || first == '-' || first == '@';
}

std::string JsonString(std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (unsigned char character : value) {
        switch (character) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (character < 0x20) {
                    escaped += "\\u00";
                    escaped.push_back(hex[character >> 4]);
                    escaped.push_back(hex[character & 0x0f]);
                } else {
                    escaped.push_back(static_cast<char>(character));
                }
        }
    }
    escaped.push_back('"');
    return escaped;
}

std::string CsvField(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 3);
    if (IsFormulaLike(value)) escaped.push_back('\'');
    for (char character : value) {
        if (character == '"') escaped.push_back('"');
        escaped.push_back(character);
    }
    return "\"" + escaped + "\"";
}

std::string TableCell(std::string_view value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string escaped;
    escaped.reserve(value.size());
    for (unsigned char character : value) {
        if (character == '\\') {
            escaped += "\\\\";
        } else if (character == '|') {
            escaped += "\\|";
        } else if (character < 0x20 || character == 0x7f) {
            escaped += "\\x";
            escaped.push_back(hex[character >> 4]);
            escaped.push_back(hex[character & 0x0f]);
        } else {
            escaped.push_back(static_cast<char>(character));
        }
    }
    return escaped;
}

} // namespace

EventOutput::EventOutput(OutputFormat format, OutputSink sink)
    : format_(format), sink_(std::move(sink)) {}

bool EventOutput::Begin(std::string& error) {
    if (began_) {
        error = "event output was already started.";
        return false;
    }
    began_ = true;
    switch (format_) {
        case OutputFormat::Json:
            return Emit("[\r\n", error);
        case OutputFormat::Csv:
            return Emit("timestamp,severity,event_id,source,summary\r\n", error);
        case OutputFormat::Table:
            return Emit(
                "Timestamp | Severity | EventID | Source | Summary\r\n"
                "----------+----------+---------+--------+--------\r\n",
                error);
    }
    error = "unsupported event output format.";
    return false;
}

bool EventOutput::Write(const EventEntry& entry, std::string& error) {
    if (!began_ || finished_) {
        error = "event output is not open.";
        return false;
    }

    std::string line;
    if (format_ == OutputFormat::Json) {
        if (!firstEntry_) line += ",\r\n";
        line += "  {\"timestamp\":" + JsonString(entry.timestamp) +
            ",\"severity\":" + JsonString(entry.severity) +
            ",\"event_id\":" + std::to_string(entry.eventId) +
            ",\"source\":" + JsonString(entry.source) +
            ",\"summary\":" + JsonString(entry.summary) + "}";
    } else if (format_ == OutputFormat::Csv) {
        line = CsvField(entry.timestamp) + "," + CsvField(entry.severity) +
            "," + std::to_string(entry.eventId) + "," +
            CsvField(entry.source) + "," + CsvField(entry.summary) + "\r\n";
    } else {
        line = TableCell(entry.timestamp) + " | " +
            TableCell(entry.severity) + " | " +
            std::to_string(entry.eventId) + " | " +
            TableCell(entry.source) + " | " +
            TableCell(entry.summary) + "\r\n";
    }

    if (!Emit(line, error)) return false;
    firstEntry_ = false;
    return true;
}

bool EventOutput::Finish(std::string& error) {
    if (!began_ || finished_) {
        error = "event output cannot be finalized.";
        return false;
    }
    if (format_ == OutputFormat::Json && !Emit("\r\n]\r\n", error)) {
        return false;
    }
    finished_ = true;
    return true;
}

bool EventOutput::Emit(std::string_view value, std::string& error) {
    if (sink_(value, error)) return true;
    if (error.empty()) error = "failed while writing event output.";
    return false;
}
