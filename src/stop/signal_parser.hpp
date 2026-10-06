#pragma once

#include "stop.hpp"

class SignalParser {
public:
    static std::string ToUpper(std::string s);

    static bool Parse(const std::string& arg, SignalType& outSignal);

    static void PrintSignalList();
};
