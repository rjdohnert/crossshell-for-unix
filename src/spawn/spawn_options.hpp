#ifndef SPAWN_OPTIONS_HPP
#define SPAWN_OPTIONS_HPP

#include "spawn.hpp"

class SpawnOptions {
public:
    bool showHelp{false};
    bool showVersion{false};
    bool wait{true};                    // Default: /WAIT; /NOWAIT for background
    bool notify{false};                 // /NOTIFY
    std::wstring processName{};          // /PROCESS_NAME="Name"
    std::wstring inputFile{};            // /INPUT=file.dat
    std::wstring outputFile{};           // /OUTPUT=file.log
    std::wstring commandString{};        // The command string or subshell
    int outputFormat{0};
    std::wstring pipeCommand;

    static void printHelp();
    static void printVersion();
    static bool parse(int argc, wchar_t* argv[], SpawnOptions& opts);
};

#endif // SPAWN_OPTIONS_HPP
