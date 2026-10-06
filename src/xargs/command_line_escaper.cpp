#include "command_line_escaper.hpp"

std::string CommandLineEscaper::escape(const std::string& arg) {
        if (arg.empty()) return "\"\"";
        if (arg.find_first_of(" \t\n\v\"") == std::string::npos) return arg;

        std::string escaped = "\"";
        for (size_t i = 0; i < arg.length(); ++i) {
            size_t backslashes = 0;
            while (i < arg.length() && arg[i] == '\\') {
                backslashes++;
                i++;
            }

            if (i == arg.length()) {
                escaped.append(backslashes * 2, '\\');
            } else if (arg[i] == '"') {
                escaped.append(backslashes * 2 + 1, '\\');
                escaped.push_back('"');
            } else {
                escaped.append(backslashes, '\\');
                escaped.push_back(arg[i]);
            }
        }
        escaped.push_back('"');
        return escaped;
    }

std::string CommandLineEscaper::replaceAll(std::string str, const std::string& from, const std::string& to) {
        if (from.empty()) return str;
        size_t startPos = 0;
        while ((startPos = str.find(from, startPos)) != std::string::npos) {
            str.replace(startPos, from.length(), to);
            startPos += to.length();
        }
        return str;
    }

std::string CommandLineEscaper::build(const std::vector<std::string>& args) {
        std::string cmd;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) cmd += " ";
            cmd += escape(args[i]);
        }
        return cmd;
    }
