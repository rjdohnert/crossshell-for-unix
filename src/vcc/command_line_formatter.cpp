#include "command_line_formatter.hpp"

std::string CommandLineFormatter::Quote(const std::string& arg) {
        if (arg.empty()) return "\"\"";
        if (arg.find_first_of(" \t\n\v\"") == std::string::npos) return arg;

        std::string out = "\"";
        int backslashes = 0;

        for (size_t i = 0; i < arg.length(); ++i) {
            if (arg[i] == '\\') {
                backslashes++;
            } else if (arg[i] == '\"') {
                out.append(backslashes * 2 + 1, '\\');
                out.push_back('\"');
                backslashes = 0;
            } else {
                out.append(backslashes, '\\');
                backslashes = 0;
                out.push_back(arg[i]);
            }
        }
        out.append(backslashes * 2, '\\');
        out += "\"";
        return out;
    }

std::string CommandLineFormatter::FormatArgs(const std::vector<std::string>& args) {
        std::ostringstream ss;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) ss << " ";
            ss << Quote(args[i]);
        }
        return ss.str();
    }
