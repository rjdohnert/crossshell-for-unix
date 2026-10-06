#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "pinky.hpp"

class PinkyOptions {
public:
    bool forceShort = false;
    bool forceLong = false;
    bool printPlan = true;
    std::vector<std::wstring> targets;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]);
    void PrintUsage(const wchar_t* exe) const;
    void PrintVersion() const;
};

#endif // OPTIONS_HPP
