#include "grammar_helpers.hpp"

void EnableVT100Colors() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
#endif
}

std::string Trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

std::string GetBaseFilename(const std::string& filepath) {
    size_t last_slash = filepath.find_last_of("/\\");
    std::string filename = (last_slash == std::string::npos) ? filepath : filepath.substr(last_slash + 1);
    size_t last_dot = filename.find_last_of('.');
    if (last_dot != std::string::npos) {
        return filename.substr(0, last_dot);
    }
    return filename;
}

std::string ExtractAction(const std::string& text, size_t& pos) {
    size_t start = pos;
    int depth = 0;
    bool in_string = false;
    bool in_char = false;
    bool in_sline_comment = false;
    bool in_mline_comment = false;

    while (pos < text.length()) {
        char c = text[pos];
        char next = (pos + 1 < text.length()) ? text[pos + 1] : '\0';

        if (in_sline_comment) {
            if (c == '\n') in_sline_comment = false;
        } else if (in_mline_comment) {
            if (c == '*' && next == '/') {
                in_mline_comment = false;
                pos++;
            }
        } else if (in_string) {
            if (c == '\\' && pos + 1 < text.length()) pos++;
            else if (c == '"') in_string = false;
        } else if (in_char) {
            if (c == '\\' && pos + 1 < text.length()) pos++;
            else if (c == '\'') in_char = false;
        } else {
            if (c == '/' && next == '/') {
                in_sline_comment = true;
                pos++;
            } else if (c == '/' && next == '*') {
                in_mline_comment = true;
                pos++;
            } else if (c == '"') {
                in_string = true;
            } else if (c == '\'') {
                in_char = true;
            } else if (c == '{') {
                depth++;
            } else if (c == '}') {
                depth--;
                if (depth == 0) {
                    pos++;
                    return text.substr(start + 1, pos - start - 2);
                }
            }
        }
        pos++;
    }
    return text.substr(start + 1);
}
