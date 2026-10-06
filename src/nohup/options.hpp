#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "nohup.hpp"

class NohupOptions {
public:
    bool showHelp = false;
    bool showVersion = false;
    int commandIndex = -1;

    bool Parse(int argc, wchar_t* argv[]);
    void PrintHelp() const;
    void PrintVersion() const;
};

#endif // OPTIONS_HPP
