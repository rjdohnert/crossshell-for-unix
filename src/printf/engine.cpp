#include "engine.hpp"

// ----------------------------------------------------------------------------
// EscapeSequenceParser
// ----------------------------------------------------------------------------
char EscapeSequenceParser::ParseEscapeSequence(const std::string& str, size_t& i) {
    if (i >= str.length()) return '\\';
    char c = str[i++];
    switch (c) {
        case 'a': return '\a';
        case 'b': return '\b';
        case 'e': case 'E': return '\x1B';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'v': return '\v';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        case '?': return '?';
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7': {
            int val = c - '0';
            int count = 0;
            while (i < str.length() && count < 2 && str[i] >= '0' && str[i] <= '7') {
                val = val * 8 + (str[i++] - '0');
                count++;
            }
            return static_cast<char>(val);
        }
        case 'x': {
            int val = 0;
            int count = 0;
            while (i < str.length() && count < 2 && std::isxdigit(static_cast<unsigned char>(str[i]))) {
                char h = str[i++];
                int digit = 0;
                if (h >= '0' && h <= '9') digit = h - '0';
                else if (h >= 'a' && h <= 'f') digit = h - 'a' + 10;
                else if (h >= 'A' && h <= 'F') digit = h - 'A' + 10;
                val = val * 16 + digit;
                count++;
            }
            return static_cast<char>(val);
        }
        default:
            return c;
    }
}

// ----------------------------------------------------------------------------
// TypeCoercer
// ----------------------------------------------------------------------------
long long TypeCoercer::ParseIntArg(const std::string& str) {
    if (str.empty()) return 0;
    if (str[0] == '\'' || str[0] == '"') {
        if (str.length() > 1) return static_cast<unsigned char>(str[1]);
        return 0;
    }
    try {
        size_t idx = 0;
        return std::stoll(str, &idx, 0);
    } catch (...) {
        return 0;
    }
}

double TypeCoercer::ParseDoubleArg(const std::string& str) {
    if (str.empty()) return 0.0;
    if (str[0] == '\'' || str[0] == '"') {
        if (str.length() > 1) return static_cast<double>(static_cast<unsigned char>(str[1]));
        return 0.0;
    }
    try {
        return std::stod(str);
    } catch (...) {
        return 0.0;
    }
}

time_t TypeCoercer::ParseTimeArg(const std::string& str) {
    if (str.empty() || str == "-1" || str == "now") {
        return std::time(nullptr);
    }
    if (str == "-2") {
        return 0;
    }
    try {
        return static_cast<time_t>(std::stoll(str, nullptr, 0));
    } catch (...) {
        return std::time(nullptr);
    }
}

// ----------------------------------------------------------------------------
// ArgumentQuoter
// ----------------------------------------------------------------------------
std::string ArgumentQuoter::QuoteArg(const std::string& arg) {
    if (arg.empty()) return "\"\"";
    bool needs_quotes = false;
    for (char c : arg) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == '&' || c == '|' ||
            c == '<' || c == '>' || c == '^' || c == '"' || c == '%') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) return arg;

    std::string quoted = "\"";
    for (char c : arg) {
        if (c == '"') quoted += "\"\"";
        else quoted += c;
    }
    quoted += "\"";
    return quoted;
}

// ----------------------------------------------------------------------------
// FormatEngine
// ----------------------------------------------------------------------------
bool FormatEngine::ProcessPercentB(const std::string& arg, std::ostream& out, bool& halt_output) {
    std::string result;
    size_t i = 0;
    while (i < arg.length()) {
        if (arg[i] == '\\') {
            i++;
            if (i >= arg.length()) {
                result += '\\';
                break;
            }
            if (arg[i] == 'c') {
                halt_output = true;
                out << result;
                return false;
            }
            result += EscapeSequenceParser::ParseEscapeSequence(arg, i);
        } else {
            result += arg[i++];
        }
    }
    out << result;
    return true;
}

void FormatEngine::ProcessTimeFormat(const std::string& timeFmt, const std::string& arg_str, std::ostream& out) {
    time_t t = TypeCoercer::ParseTimeArg(arg_str);
    std::tm tm_buf;
    localtime_s(&tm_buf, &t);
    char buf[256];
    std::string actualFmt = timeFmt.empty() ? "%c" : timeFmt;
    size_t len = std::strftime(buf, sizeof(buf), actualFmt.c_str(), &tm_buf);
    if (len > 0) {
        out << buf;
    }
}

void FormatEngine::ProcessSpecifier(const std::string& fmt, size_t& i,
                             const std::vector<std::string>& args, size_t& arg_index,
                             std::ostream& out, bool& halt_output) {
    size_t start = i - 1;

    if (i < fmt.length() && fmt[i] == '(') {
        size_t closeParen = fmt.find(')', i);
        if (closeParen != std::string::npos && closeParen + 1 < fmt.length() && fmt[closeParen + 1] == 'T') {
            std::string datePattern = fmt.substr(i + 1, closeParen - (i + 1));
            i = closeParen + 2;
            std::string arg_str = (arg_index < args.size()) ? args[arg_index++] : "-1";
            ProcessTimeFormat(datePattern, arg_str, out);
            return;
        }
    }

    while (i < fmt.length() && (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == ' ' || fmt[i] == '#' || fmt[i] == '0' || fmt[i] == '\'')) {
        i++;
    }

    bool width_from_arg = false;
    if (i < fmt.length() && fmt[i] == '*') {
        width_from_arg = true;
        i++;
    } else {
        while (i < fmt.length() && std::isdigit(static_cast<unsigned char>(fmt[i]))) {
            i++;
        }
    }

    bool precision_from_arg = false;
    if (i < fmt.length() && fmt[i] == '.') {
        i++;
        if (i < fmt.length() && fmt[i] == '*') {
            precision_from_arg = true;
            i++;
        } else {
            while (i < fmt.length() && std::isdigit(static_cast<unsigned char>(fmt[i]))) {
                i++;
            }
        }
    }

    while (i < fmt.length() && (fmt[i] == 'h' || fmt[i] == 'l' || fmt[i] == 'L' || fmt[i] == 'z' || fmt[i] == 'j' || fmt[i] == 't')) {
        i++;
    }

    if (i >= fmt.length()) {
        out << '%';
        return;
    }

    char spec = fmt[i++];

    int star_width = 0;
    if (width_from_arg) {
        std::string w_str = (arg_index < args.size()) ? args[arg_index++] : "0";
        star_width = static_cast<int>(TypeCoercer::ParseIntArg(w_str));
    }

    int star_precision = 0;
    if (precision_from_arg) {
        std::string p_str = (arg_index < args.size()) ? args[arg_index++] : "0";
        star_precision = static_cast<int>(TypeCoercer::ParseIntArg(p_str));
    }

    std::string arg_str = (arg_index < args.size()) ? args[arg_index++] : "";
    std::string spec_str = fmt.substr(start, i - start);

    if (width_from_arg || precision_from_arg) {
        std::string rebuilt = "%";
        size_t p = start + 1;
        while (p < fmt.length() && (fmt[p] == '-' || fmt[p] == '+' || fmt[p] == ' ' || fmt[p] == '#' || fmt[p] == '0' || fmt[p] == '\'')) {
            rebuilt += fmt[p++];
        }
        if (width_from_arg) {
            rebuilt += std::to_string(star_width);
            p++;
        } else {
            while (p < fmt.length() && std::isdigit(static_cast<unsigned char>(fmt[p]))) {
                rebuilt += fmt[p++];
            }
        }
        if (p < fmt.length() && fmt[p] == '.') {
            rebuilt += '.';
            p++;
            if (precision_from_arg) {
                rebuilt += std::to_string(star_precision);
                p++;
            } else {
                while (p < fmt.length() && std::isdigit(static_cast<unsigned char>(fmt[p]))) {
                    rebuilt += fmt[p++];
                }
            }
        }
        while (p < fmt.length() && (fmt[p] == 'h' || fmt[p] == 'l' || fmt[p] == 'L' || fmt[p] == 'z' || fmt[p] == 'j' || fmt[p] == 't')) {
            p++;
        }
        rebuilt += spec;
        spec_str = rebuilt;
    }

    if (spec == 'T') {
        ProcessTimeFormat("%c", arg_str, out);
        return;
    }

    if (spec == 'b') {
        ProcessPercentB(arg_str, out, halt_output);
        return;
    }

    if (spec == 'q') {
        std::string q = ArgumentQuoter::QuoteArg(arg_str);
        if (!spec_str.empty()) {
            spec_str.back() = 's';
        }
        char buf[2048];
        snprintf(buf, sizeof(buf), spec_str.c_str(), q.c_str());
        out << buf;
        return;
    }

    char buf[2048];
    if (spec == 'd' || spec == 'i') {
        spec_str.insert(spec_str.length() - 1, "ll");
        long long val = TypeCoercer::ParseIntArg(arg_str);
        snprintf(buf, sizeof(buf), spec_str.c_str(), val);
        out << buf;
    } else if (spec == 'o' || spec == 'u' || spec == 'x' || spec == 'X') {
        spec_str.insert(spec_str.length() - 1, "ll");
        unsigned long long val = static_cast<unsigned long long>(TypeCoercer::ParseIntArg(arg_str));
        snprintf(buf, sizeof(buf), spec_str.c_str(), val);
        out << buf;
    } else if (spec == 'f' || spec == 'F' || spec == 'e' || spec == 'E' || spec == 'g' || spec == 'G' || spec == 'a' || spec == 'A') {
        double val = TypeCoercer::ParseDoubleArg(arg_str);
        snprintf(buf, sizeof(buf), spec_str.c_str(), val);
        out << buf;
    } else if (spec == 's') {
        snprintf(buf, sizeof(buf), spec_str.c_str(), arg_str.c_str());
        out << buf;
    } else if (spec == 'c') {
        char c = arg_str.empty() ? '\0' : (arg_str[0] == '\'' || arg_str[0] == '"') && arg_str.length() > 1 ? arg_str[1] : arg_str[0];
        snprintf(buf, sizeof(buf), spec_str.c_str(), c);
        out << buf;
    } else if (spec == 'p') {
        unsigned long long val = static_cast<unsigned long long>(TypeCoercer::ParseIntArg(arg_str));
        snprintf(buf, sizeof(buf), spec_str.c_str(), reinterpret_cast<void*>(val));
        out << buf;
    } else {
        out << spec_str;
    }
}

void FormatEngine::ExecuteFormatLoop(const std::string& format, const std::vector<std::string>& args, std::ostream& out) {
    size_t arg_index = 0;
    bool halt_output = false;

    do {
        size_t prev_arg_index = arg_index;

        for (size_t i = 0; i < format.length(); ) {
            if (halt_output) break;

            if (format[i] == '\\') {
                i++;
                char c = EscapeSequenceParser::ParseEscapeSequence(format, i);
                out << c;
            } else if (format[i] == '%') {
                i++;
                if (i < format.length() && format[i] == '%') {
                    out << '%';
                    i++;
                    continue;
                }
                ProcessSpecifier(format, i, args, arg_index, out, halt_output);
            } else {
                out << format[i++];
            }
        }

        if (arg_index == prev_arg_index && arg_index >= args.size()) {
            break;
        }

    } while (arg_index < args.size() && !halt_output);
}
