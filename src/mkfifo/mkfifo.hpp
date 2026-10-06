#ifndef MKFIFO_HPP
#define MKFIFO_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <iomanip>
#include <sstream>
#include <csignal>
#include <atomic>

#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;

extern std::atomic<bool> g_running;
void signalHandler(int);

class WindowsPipeMode {
public:
    enum class Direction {
        Duplex,
        Inbound,
        Outbound
    };

    enum class Type {
        Byte,
        Message
    };

    Direction direction = Direction::Duplex;
    Type type = Type::Byte;
    std::string sddlString = "D:(A;;GRGW;;;WD)";

    DWORD getOpenMode() const;
    DWORD getPipeMode() const;
    std::string getDirectionString() const;
    std::string getTypeString() const;
};

class FifoOptions {
public:
    WindowsPipeMode mode;
    bool persist = false;
    bool verbose = false;
    uint32_t bufferSize = 8192;
    std::vector<std::string> pipeNames;
    bool showHelp = false;
    bool showVersion = false;
};

#endif // MKFIFO_HPP
