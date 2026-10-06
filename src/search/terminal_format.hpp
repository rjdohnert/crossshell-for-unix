#ifndef TERMINAL_FORMAT_HPP
#define TERMINAL_FORMAT_HPP

#include "search.hpp"

class ConsoleTerminal {
public:
    inline static const std::string Reset   = "\033[0m";
    inline static const std::string Bold    = "\033[1m";
    inline static const std::string Dim     = "\033[2m";
    inline static const std::string Red     = "\033[91m";
    inline static const std::string Green   = "\033[92m";
    inline static const std::string Yellow  = "\033[93m";
    inline static const std::string Blue    = "\033[94m";
    inline static const std::string Magenta = "\033[95m";
    inline static const std::string Cyan    = "\033[96m";
    inline static const std::string Gray    = "\033[90m";

    static void EnableVirtualTerminal();
    static std::string FormatTypeBadge(bool isDir, bool isSym);
};

class SizeFormatter {
public:
    static std::optional<uintmax_t> Parse(const std::string& str);
    static std::string Format(uintmax_t bytes);
};

class DateTimeFormatter {
public:
    static std::chrono::system_clock::time_point ToSystemTime(fs::file_time_type ftime);
    static std::optional<std::chrono::system_clock::time_point> Parse(const std::string& str);
    static std::string Format(fs::file_time_type ftime);
};

class AttributeInspector {
public:
    static std::string GetAttributesString(const fs::path& p, DWORD* rawAttr = nullptr);
};

#endif // TERMINAL_FORMAT_HPP
