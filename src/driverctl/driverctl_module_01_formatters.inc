// C++17-compatible replacement for std::format with sequential {} placeholders.
// Writes into a single reusable grow-only buffer instead of constructing an
// std::ostringstream (with its locale machinery + internal allocation) per call.
namespace detail {
    // Thread-local reusable output buffer; grows to the largest line and is
    // reused for every Fmt call, eliminating per-line heap churn.
    inline std::string& FmtBuffer() {
        thread_local std::string buf;
        return buf;
    }

    // Append a single argument to the buffer. Strings copy directly; integrals
    // format via snprintf into a small stack scratch (no iostream involved).
    inline void AppendArg(std::string& out, const std::string& v) { out += v; }
    inline void AppendArg(std::string& out, std::string_view v)     { out.append(v.data(), v.size()); }
    inline void AppendArg(std::string& out, const char* v)          { out += v; }
    inline void AppendArg(std::string& out, char v)                 { out += v; }

    template <typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, char>::value, int>::type = 0>
    inline void AppendArg(std::string& out, T v) {
        char scratch[32];
        int n = 0;
        if constexpr (std::is_signed<T>::value)
            n = snprintf(scratch, sizeof(scratch), "%lld", static_cast<long long>(v));
        else
            n = snprintf(scratch, sizeof(scratch), "%llu", static_cast<unsigned long long>(v));
        if (n > 0) out.append(scratch, static_cast<size_t>(n));
    }

    inline void FmtImpl(std::string& out, const char* fmt) {
        out += fmt; // no remaining args; emit the rest verbatim
    }
    template <typename T, typename... Rest>
    void FmtImpl(std::string& out, const char* fmt, T&& value, Rest&&... rest) {
        for (const char* p = fmt; *p; ++p) {
            if (p[0] == '{' && p[1] == '}') {
                AppendArg(out, std::forward<T>(value));
                FmtImpl(out, p + 2, std::forward<Rest>(rest)...);
                return;
            }
            out += *p;
        }
    }
}

// Formats into the reusable buffer and returns a reference to it. Callers pass
// the result straight to WriteLine, so no per-call std::string is returned.
// NOTE: the returned view is invalidated by the next Fmt call on this thread.
template <typename... Args>
const std::string& Fmt(const std::string& fmt, Args&&... args) {
    std::string& out = detail::FmtBuffer();
    out.clear();
    detail::FmtImpl(out, fmt.c_str(), std::forward<Args>(args)...);
    return out;
}

// Joins a list of strings with ", " for compact report display.
std::string JoinList(const std::vector<std::string>& items) {
    std::ostringstream oss;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << items[i];
    }
    return oss.str();
}

// Output formats for pipeline-friendly rendering.
enum class OutputFormat {
    Report,   // default human-readable diagnostic report (console + log file)
    Table,    // aligned tabular columns on stdout
    Json,     // JSON array on stdout
    Csv       // CSV rows on stdout
};

// Escapes a string for embedding in a JSON string literal.
std::string EscapeJSON(const std::string& s) {
    std::string res;
    for (char c : s) {
        switch (c) {
            case '\\': res += "\\\\"; break;
            case '"':  res += "\\\""; break;
            case '\n': res += "\\n";  break;
            case '\r': res += "\\r";  break;
            case '\t': res += "\\t";  break;
            default:    res += c;
        }
    }
    return res;
}

// Escapes a field for CSV (wraps in quotes, doubles internal quotes).
std::string EscapeCSV(const std::string& s) {
    std::string res = "\"";
    for (char c : s) {
        if (c == '"') res += "\"\"";
        else res += c;
    }
    res += "\"";
    return res;
}
