#include "ipc_command_parser.hpp"
#include "supervisor_defaults.hpp"

std::vector<std::string> ParseIpcCommandTokens(const std::string& input) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    bool escaping = false;

    for (char ch : input) {
        if (escaping) {
            current.push_back(ch);
            escaping = false;
            continue;
        }

        if (ch == '\\') {
            escaping = true;
            continue;
        }

        if (ch == '"') {
            inQuotes = !inQuotes;
            continue;
        }

        if (!inQuotes && std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current.push_back(ch);
    }

    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

std::string QuoteForIpc(const std::string& value) {
    if (value.find_first_of(" \t\"") == std::string::npos) return value;
    std::string out = "\"";
    for (char ch : value) {
        if (ch == '\\' || ch == '"') out.push_back('\\');
        out.push_back(ch);
    }
    out.push_back('"');
    return out;
}
