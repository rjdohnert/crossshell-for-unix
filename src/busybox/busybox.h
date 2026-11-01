#ifndef BUSYBOX_H
#define BUSYBOX_H

// Shared umbrella header for the busybox multi-call binary: platform includes,
// resource-limit constants, and the ShellExitSignal exception type.

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <lmcons.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <filesystem>
#include <algorithm>
#include <atomic>
#include <iomanip>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>
#include <system_error>
#include <memory>
#include <regex>
#include <mutex>
#include <initializer_list>
#include <TlHelp32.h>
#include <wincrypt.h>
#include <conio.h>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Crypt32.lib")

namespace fs = std::filesystem;

#define BUSYBOX_VERSION "8.2"
#define BUSYBOX_COPYRIGHT "Copyright (C) 2026, Roberto J. Dohnert"

constexpr size_t MAX_AST_RECURSION_DEPTH = 128;
constexpr size_t MAX_HISTORY_ENTRIES = 4096;
constexpr size_t MAX_HISTORY_LINE_BYTES = 16 * 1024;
constexpr size_t MAX_HISTORY_FILE_BYTES = 4 * 1024 * 1024;
constexpr size_t MAX_SCRIPT_BYTES = 16 * 1024 * 1024;
constexpr size_t MAX_COMMAND_SUBSTITUTION_BYTES = 8 * 1024 * 1024;
constexpr size_t MAX_GLOB_MATCHES = 100000;
constexpr size_t MAX_DIRECTORY_SCAN_ENTRIES = 1000000;

class ShellExitSignal : public std::exception {
public:
    explicit ShellExitSignal(int code) : code_(code) {}
    int code() const noexcept { return code_; }
private:
    int code_;
};

#endif // BUSYBOX_H
