#include "powershell_quoting.hpp"

std::string escape_ps_single_quotes(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\'') {
            out += "''";
        } else {
            out += c;
        }
    }
    return out;
}

std::string ps_quote(const std::string& s) {
    return "'" + escape_ps_single_quotes(s) + "'";
}
