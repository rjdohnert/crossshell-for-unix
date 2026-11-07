#ifndef DRIVERCTL_FORMATTERS_HPP
#define DRIVERCTL_FORMATTERS_HPP

#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace DriverCtlFormatDetail {
inline void AppendArgument(std::string& output, const std::string& value) {
    output += value;
}

inline void AppendArgument(std::string& output, std::string_view value) {
    if (!value.empty()) output.append(value.data(), value.size());
}

inline void AppendArgument(std::string& output, const char* value) {
    if (value) output += value;
}

inline void AppendArgument(std::string& output, char value) {
    output += value;
}

template <typename T,
          typename std::enable_if<std::is_integral<T>::value &&
                                  !std::is_same<T, char>::value, int>::type = 0>
inline void AppendArgument(std::string& output, T value) {
    char buffer[32];
    int length;
    if constexpr (std::is_signed<T>::value) {
        length = std::snprintf(buffer, sizeof(buffer), "%lld",
                               static_cast<long long>(value));
    } else {
        length = std::snprintf(buffer, sizeof(buffer), "%llu",
                               static_cast<unsigned long long>(value));
    }
    if (length > 0 && static_cast<size_t>(length) < sizeof(buffer)) {
        output.append(buffer, static_cast<size_t>(length));
    }
}

inline void FormatInto(std::string& output, const char* format) {
    output += format;
}

template <typename T, typename... Rest>
void FormatInto(std::string& output, const char* format,
                T&& value, Rest&&... rest) {
    for (const char* current = format; *current; ++current) {
        if (current[0] == '{' && current[1] == '}') {
            AppendArgument(output, std::forward<T>(value));
            FormatInto(output, current + 2, std::forward<Rest>(rest)...);
            return;
        }
        output += *current;
    }
}
}

template <typename... Args>
std::string Fmt(const std::string& format, Args&&... args) {
    std::string output;
    output.reserve(format.size() + sizeof...(Args) * 16);
    DriverCtlFormatDetail::FormatInto(output, format.c_str(),
                                      std::forward<Args>(args)...);
    return output;
}

std::string JoinList(const std::vector<std::string>& items);
std::string EscapeJSON(const std::string& value);
std::string EscapeCSV(const std::string& value);

enum class OutputFormat {
    Report,
    Table,
    Json,
    Csv
};

#endif
