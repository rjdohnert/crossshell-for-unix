#ifndef SEQUENCE_HPP
#define SEQUENCE_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <random>
#include <chrono>
#include <unordered_map>
#include <optional>
#include <memory>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

struct KeyFlags {
    bool numeric{false};             // -n
    bool humanNumeric{false};        // -h
    bool generalNumeric{false};      // -g
    bool month{false};               // -M
    bool version{false};             // -V
    bool randomSort{false};          // -R
    bool reverse{false};             // -r
    bool ignoreCase{false};          // -f
    bool dictOrder{false};           // -d
    bool ignoreNonPrint{false};      // -i
    bool ignoreLeadingBlanks{false}; // -b
};

class KeyDefinition {
public:
    int startField{1};
    int startChar{1};
    int endField{0}; // 0 = end of line
    int endChar{0};  // 0 = end of field
    KeyFlags flags;
    bool hasCustomFlags{false};

    static bool parse(const std::string& def, KeyDefinition& outKey, const KeyFlags& globalFlags);
};

#endif // SEQUENCE_HPP
