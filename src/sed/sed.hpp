#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <regex>
#include <memory>
#include <cctype>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <cerrno>
#include <cstdlib>
#include <limits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

namespace fs = std::filesystem;

namespace SedSafe {
    inline bool ParseInteger(const std::string& text, int& value) {
        if (text.empty()) return false;
        char* end = nullptr;
        errno = 0;
        long parsed = std::strtol(text.c_str(), &end, 10);
        if (errno != 0 || end == text.c_str() || *end != '\0') return false;
        if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) return false;
        value = static_cast<int>(parsed);
        return true;
    }

    inline bool CompileRegex(const std::string& pattern, std::regex& re, std::regex::flag_type flags = std::regex::ECMAScript) {
        if (pattern.size() > 4096) return false;
        try {
            re = std::regex(pattern, flags);
            return true;
        } catch (const std::regex_error&) {
            return false;
        }
    }
}

// Embedded JSON Engine types
struct Value;
using Object = std::map<std::string, Value>;
using Array  = std::vector<Value>;

struct Value {
    enum Type { NIL, BOOL, NUMBER, STRING, ARRAY, OBJECT } type = NIL;
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data;

    Value() : type(NIL), data(nullptr) {}
    Value(bool b) : type(BOOL), data(b) {}
    Value(double d) : type(NUMBER), data(d) {}
    Value(const std::string& s) : type(STRING), data(s) {}
    Value(const char* s) : type(STRING), data(std::string(s)) {}
    Value(Array a) : type(ARRAY), data(std::move(a)) {}
    Value(Object o) : type(OBJECT), data(std::move(o)) {}

    std::string to_string() const;
    double to_number() const;
    bool to_bool() const;
    std::string serialize_json() const;
    Value* get_path_mut(const std::string& key);
    const Value* get_path(const std::string& key) const;
    bool erase_path(const std::string& key);
    Value* resolve_ptr(const std::string& path_str, bool create_missing = false);
    bool del_path(const std::string& path_str);
};

Value parse_scalar_literal(const std::string& raw_val);

enum class CommandType {
    SUBSTITUTE,     // s/re/rep/flags or s/.path/re/rep/flags
    SET_PROPERTY,   // set .path = value
    DEL_PROPERTY,   // del .path
    DELETE_LINE,    // d
    PRINT_LINE,     // p
    QUIT            // q
};

struct Address {
    enum Type { ALL, SINGLE_LINE, LINE_RANGE, REGEX_MATCH, PREDICATE } type = ALL;
    long long line1 = 0;
    long long line2 = 0;
    std::string regex_str = "";
    std::string predicate_str = "";
};

struct SedCommand {
    Address addr;
    CommandType type = CommandType::PRINT_LINE;
    std::string path = "";           // Object path for .key operations
    std::string find_regex = "";     // Regex to match
    std::string replacement = "";    // Replacement string
    Value set_val;                   // Literal value for `set`
    bool flag_global = false;        // 'g' flag
    bool flag_ignore_case = false;   // 'i' flag
    bool flag_print = false;         // 'p' flag
};

struct SedOptions {
    bool quiet = false;              // -n / --quiet
    bool in_place = false;           // -i / --in-place
    bool extended_regex = false;     // -E / -r
    bool separate_files = false;     // -s
    bool unbuffered = false;         // -u
    bool binary_mode = false;        // -b
    bool posix_mode = false;         // --posix
    bool sandbox_mode = false;       // --sandbox
    bool debug_mode = false;         // --debug
    bool follow_symlinks = false;    // --follow-symlinks
    bool end_of_options = false;     // --
    std::string backup_suffix = "";  // Backup suffix (e.g. .bak)
    bool json_only = false;          // -j
    bool text_only = false;          // -t
    char record_delim = '\n';        // -0 / --null-data
    std::vector<std::string> scripts;
    std::vector<std::string> files;
    std::vector<std::string> object_sources;
};
