#include "json_formatter.hpp"
#include "process_thread_info.hpp"

void JsonFormatter::format(std::ostream& os, const std::vector<ProcessThreadInfo>& data)  {
        os << "[\n";
        for (size_t i = 0; i < data.size(); ++i) {
            const auto& item = data[i];
            os << "  {\n";
            os << "    \"pid\": " << item.pid << ",\n";
            os << "    \"threadCount\": " << item.threadCount << ",\n";
            os << "    \"processName\": \"" << escapeJson(item.narrowName()) << "\"\n";
            os << "  }" << (i + 1 < data.size() ? "," : "") << "\n";
        }
        os << "]\n";
    }

std::string JsonFormatter::escapeJson(const std::string& str) {
        std::ostringstream ss;
        for (char c : str) {
            switch (c) {
                case '\"': ss << "\\\""; break;
                case '\\': ss << "\\\\"; break;
                case '\b': ss << "\\b"; break;
                case '\f': ss << "\\f"; break;
                case '\n': ss << "\\n"; break;
                case '\r': ss << "\\r"; break;
                case '\t': ss << "\\t"; break;
                default:   ss << c; break;
            }
        }
        return ss.str();
    }
