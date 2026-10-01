/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * ============================================================================
 * SINGLE FILE INDEX: wall.cpp
 * ============================================================================
 * WinWall - Object-Oriented Terminal Broadcast System for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 * Win32 SDK: wtsapi32.lib, user32.lib, advapi32.lib
 *
 * TABLE OF CONTENTS:
 * 1. [CONFIGURATION & OPTIONS] ............. WallConfig class (CLI parsing & options)
 * 2. [SYSTEM INFO & BANNER FORMATTER] ...... SystemInfoProvider and BannerFormatter classes
 * 3. [SESSION BROADCASTER] ................. SessionBroadcaster class (WTS / GUI / Conhost)
 * 4. [CORE BROADCAST ENGINE] ............... WallEngine class (I/O, message build, dispatch)
 * 5. [APPLICATION CONTROLLER] .............. WallApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wtsapi32.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <lmcons.h>
#include <memory>

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

// ============================================================================
// 1. CONFIGURATION & OPTIONS
// ============================================================================

class WallConfig {
public:
    static constexpr size_t MAX_MSG_LIMIT = 500;
    static constexpr const char* VERSION = "1.0.0";

    bool showHelp{false};
    bool showVersion{false};
    bool noBanner{false};
    bool guiPopup{false};
    bool useConhost{false};
    std::string filePath{""};
    std::string inlineMessage{""};
    std::string serverName{""};
    int outputFormat{0}; // 0: raw, 1: JSON, 2: CSV, 3: Table
    std::string pipeCommand;

    static void printHelp(const char* exeName) {
        std::cout <<
R"(NAME
    wall - write a message to all logged-in users on Windows

SYNOPSIS
    )" << exeName << R"( [OPTIONS] [FILE]
    )" << exeName << R"( [OPTIONS] [MESSAGE...]
    echo "Message" | )" << exeName << R"( [OPTIONS]

DESCRIPTION
    wall displays a message, or the contents of a file, or lines taken from its
    standard input, on the terminals and active sessions of all currently logged-in
    users.

OPTIONS
    -n, --nobanner
        Suppress the standard header banner showing user, host, and time.
    -g, --gui
        Display the message as an interactive graphical dialog on user desktops.
    -c, --conhost
        Force delivery directly to active console hosts.
    --server SERVER
        Broadcast to a remote Terminal Server / Windows Host.
    --json, --csv, --table
        Format delivery confirmation records as JSON, CSV, or Table.
    --pipe COMMAND
        Forward delivery receipts through COMMAND.
    -h, --help
        Display this help manual.
    -V, --version
        Display version information.

EXAMPLES
    wall System going down for maintenance in 5 minutes!
    type notice.txt | wall -n
    wall -g --server RDS-HOST-01 Update scheduled at 22:00.
)";
    }

    static void printVersion() {
        std::cout << "wall version " << VERSION << "\n"
                  << "Copyright (c) 2026, Roberto J Dohnert.\n";
    }

    static bool parse(int argc, char* argv[], WallConfig& cfg) {
        std::vector<std::string> words;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printHelp(argv[0]);
                std::exit(0);
            } else if (arg == "-V" || arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-n" || arg == "--nobanner") {
                cfg.noBanner = true;
            } else if (arg == "-g" || arg == "--gui") {
                cfg.guiPopup = true;
            } else if (arg == "-c" || arg == "--conhost") {
                cfg.useConhost = true;
            } else if (arg == "--json") {
                cfg.outputFormat = 1;
            } else if (arg == "--csv") {
                cfg.outputFormat = 2;
            } else if (arg == "--table") {
                cfg.outputFormat = 3;
            } else if (arg == "--server" && i + 1 < argc) {
                cfg.serverName = argv[++i];
            } else if (arg == "--pipe" && i + 1 < argc) {
                cfg.pipeCommand = argv[++i];
            } else if (!arg.empty() && arg[0] == '-') {
                std::cerr << "wall: unrecognized option '" << arg << "'\n";
                return false;
            } else {
                words.push_back(arg);
            }
        }

        if (!words.empty()) {
            std::ifstream testFile(words[0]);
            if (words.size() == 1 && testFile.good()) {
                cfg.filePath = words[0];
            } else {
                std::ostringstream oss;
                for (size_t k = 0; k < words.size(); ++k) {
                    if (k > 0) oss << " ";
                    oss << words[k];
                }
                cfg.inlineMessage = oss.str();
            }
        }

        return true;
    }
};

// ============================================================================
// 2. SYSTEM INFO & BANNER FORMATTER
// ============================================================================

class SystemInfoProvider {
public:
    static std::wstring utf8ToWide(const std::string& str) {
        if (str.empty()) return std::wstring();
        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), NULL, 0);
        std::wstring wstrTo(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), &wstrTo[0], sizeNeeded);
        return wstrTo;
    }

    static std::string getCurrentUserName() {
        wchar_t buffer[256];
        DWORD size = sizeof(buffer) / sizeof(wchar_t);
        if (GetUserNameW(buffer, &size)) {
            char mbBuf[256];
            WideCharToMultiByte(CP_UTF8, 0, buffer, -1, mbBuf, sizeof(mbBuf), NULL, NULL);
            return std::string(mbBuf);
        }
        return "UNKNOWN_USER";
    }

    static std::string getCurrentHostName() {
        wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(buffer) / sizeof(wchar_t);
        if (GetComputerNameW(buffer, &size)) {
            char mbBuf[256];
            WideCharToMultiByte(CP_UTF8, 0, buffer, -1, mbBuf, sizeof(mbBuf), NULL, NULL);
            return std::string(mbBuf);
        }
        return "UNKNOWN_HOST";
    }

    static std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto inTimeT = std::chrono::system_clock::to_time_t(now);
        std::tm tmStruct{};
        localtime_s(&tmStruct, &inTimeT);

        std::ostringstream oss;
        oss << std::put_time(&tmStruct, "%a %b %d %H:%M:%S %Y");
        return oss.str();
    }
};

class BannerFormatter {
public:
    static std::string format(const std::string& user, const std::string& host, const std::string& timeStr) {
        std::ostringstream oss;
        oss << "\r\nBroadcast message from " << user << "@" << host << " (" << timeStr << "):\r\n\r\n";
        return oss.str();
    }
};

// ============================================================================
// 3. SESSION BROADCASTER
// ============================================================================

class SessionBroadcaster {
public:
    static bool broadcastWts(HANDLE hServer, const std::string& title, const std::string& message, bool isGui) {
        PWTS_SESSION_INFOW pSessionInfo = NULL;
        DWORD count = 0;

        if (!WTSEnumerateSessionsW(hServer, 0, 1, &pSessionInfo, &count)) {
            return false;
        }

        std::wstring wTitle = SystemInfoProvider::utf8ToWide(title);
        std::wstring wMsg = SystemInfoProvider::utf8ToWide(message);
        DWORD style = isGui ? (MB_OK | MB_ICONEXCLAMATION) : (MB_OK | MB_ICONASTERISK);
        DWORD response = 0;

        for (DWORD i = 0; i < count; ++i) {
            if (pSessionInfo[i].State == WTSActive || pSessionInfo[i].State == WTSConnected) {
                WTSSendMessageW(
                    hServer,
                    pSessionInfo[i].SessionId,
                    wTitle.data(),
                    static_cast<DWORD>(wTitle.size() * sizeof(wchar_t)),
                    wMsg.data(),
                    static_cast<DWORD>(wMsg.size() * sizeof(wchar_t)),
                    style,
                    0,
                    &response,
                    FALSE
                );
            }
        }

        WTSFreeMemory(pSessionInfo);
        return true;
    }
};

// ============================================================================
// 4. CORE BROADCAST ENGINE
// ============================================================================

class WallEngine {
private:
    WallConfig config;

public:
    explicit WallEngine(WallConfig cfg) : config(std::move(cfg)) {}

    int execute() {
        std::string rawMessage;

        if (!config.inlineMessage.empty()) {
            rawMessage = config.inlineMessage;
        } else if (!config.filePath.empty()) {
            std::ifstream file(config.filePath);
            if (!file.is_open()) {
                std::cerr << "wall: cannot open file " << config.filePath << "\n";
                return 1;
            }
            std::ostringstream ss;
            ss << file.rdbuf();
            rawMessage = ss.str();
        } else {
            std::string line;
            std::ostringstream ss;
            while (std::getline(std::cin, line)) {
                ss << line << "\r\n";
            }
            rawMessage = ss.str();
        }

        if (rawMessage.empty()) {
            std::cerr << "wall: empty message\n";
            return 1;
        }

        std::string user = SystemInfoProvider::getCurrentUserName();
        std::string host = SystemInfoProvider::getCurrentHostName();
        std::string timeStr = SystemInfoProvider::getCurrentTimestamp();

        std::string fullBroadcast;
        if (!config.noBanner) {
            fullBroadcast = BannerFormatter::format(user, host, timeStr) + rawMessage + "\r\n";
        } else {
            fullBroadcast = "\r\n" + rawMessage + "\r\n";
        }

        HANDLE hServer = WTS_CURRENT_SERVER_HANDLE;
        if (!config.serverName.empty()) {
            std::wstring wServer = SystemInfoProvider::utf8ToWide(config.serverName);
            hServer = WTSOpenServerW(wServer.data());
            if (!hServer) {
                std::cerr << "wall: failed to open server " << config.serverName << "\n";
                return 1;
            }
        }

        std::string title = "Broadcast from " + user + "@" + host;
        bool ok = SessionBroadcaster::broadcastWts(hServer, title, fullBroadcast, config.guiPopup);

        if (!ok || config.useConhost) {
            std::cout << fullBroadcast;
            std::cout.flush();
        }

        if (hServer != WTS_CURRENT_SERVER_HANDLE && hServer != NULL) {
            WTSCloseServer(hServer);
        }

        if (config.outputFormat != 0 || !config.pipeCommand.empty()) {
            std::string text;
            if (config.outputFormat == 1) {
                text = "{\"status\":\"broadcast_sent\",\"sender\":\"" + user + "\",\"host\":\"" + host + "\"}\n";
            } else if (config.outputFormat == 2) {
                text = "\"status\",\"sender\",\"host\"\n\"broadcast_sent\",\"" + user + "\",\"" + host + "\"\n";
            } else if (config.outputFormat == 3) {
                text = "STATUS\tSENDER\tHOST\n-----------------------------\nbroadcast_sent\t" + user + "\t" + host + "\n";
            }

            if (!config.pipeCommand.empty()) {
                FILE* pipe = _popen(config.pipeCommand.c_str(), "w");
                if (pipe) {
                    std::fwrite(text.data(), 1, text.size(), pipe);
                    _pclose(pipe);
                }
            } else if (config.outputFormat != 0) {
                std::cout << text;
            }
        }

        return 0;
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class WallApp {
public:
    static int run(int argc, char* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        WallConfig config;
        if (!WallConfig::parse(argc, argv, config)) {
            return 1;
        }

        WallEngine engine(std::move(config));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return WallApp::run(argc, argv);
}
