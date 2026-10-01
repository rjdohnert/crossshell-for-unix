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
 * SINGLE FILE INDEX: dircolors.cpp
 * ============================================================================
 * WinDircolors - Object-Oriented LS_COLORS Color Setup Utility for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. DircolorsOptions class (CLI parsing & shells)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... DircolorsReporter class (JSON/CSV/Table/Pipe)
 * 3. [DATABASE PARSER ENGINE] .............. ColorDatabaseParser and DircolorsEngine classes
 * 4. [APPLICATION CONTROLLER] .............. DircolorsApp class and main entry point
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

enum class TargetShell {
    Bourne,
    CShell,
    PowerShell,
    Cmd,
    PrintDatabase
};

class DircolorsOptions {
public:
    TargetShell shell{TargetShell::Bourne};
    std::string configFile{""};
    int outputFormat{0};
    std::string pipeCommand;

    static void printHelp(const char* /*progName*/ = nullptr) {
        std::cout << R"(dircolors(1)               CrossShell for UNIX Reference Manual               dircolors(1)

    NAME
        dircolors - output commands to set the LS_COLORS environment variable

    SYNOPSIS
        dircolors [OPTION]... [FILE]

    DESCRIPTION
        Output commands to set the LS_COLORS environment variable.

        If FILE is specified, read it to determine what colors to use for
        which file types and extensions. Otherwise, a precompiled database is used.

    OPTIONS
        -b, --sh, --bourne-shell
            Output Bourne shell code to set LS_COLORS.
        -c, --csh, --c-shell
            Output C shell code to set LS_COLORS.
        -p, --powershell, --ps
            Output PowerShell code to set $env:LS_COLORS.
        -cmd, --cmd
            Output Windows CMD batch code to set LS_COLORS.
        -p, --print-database
            Output default configuration database.
        --json, --csv, --table
            Output in structured format.
        --pipe COMMAND
            Send output through COMMAND.
        -h, --help
            Display this help and exit.
        --version
            Output version information and exit.

    EXAMPLES
        dircolors -b
            Output Bourne shell commands to configure LS_COLORS.

        dircolors -p
            Output PowerShell commands to configure $env:LS_COLORS.

        dircolors --print-database
            Print the internal database of default color bindings.

    CrossShell for UNIX                                                    dircolors(1)
)";
    }

    static void printVersion() {
        std::cout << "dircolors 1.0\n";
    }

    static bool parse(int argc, char* argv[], DircolorsOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h" || arg == "/?") {
                printHelp(argv[0]);
                std::exit(0);
            } else if (arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-b" || arg == "--sh" || arg == "--bourne-shell") {
                opts.shell = TargetShell::Bourne;
            } else if (arg == "-c" || arg == "--csh" || arg == "--c-shell") {
                opts.shell = TargetShell::CShell;
            } else if (arg == "--powershell" || arg == "--ps") {
                opts.shell = TargetShell::PowerShell;
            } else if (arg == "-cmd" || arg == "--cmd") {
                opts.shell = TargetShell::Cmd;
            } else if (arg == "-p" || arg == "--print-database") {
                opts.shell = TargetShell::PrintDatabase;
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg[0] == '-' && arg.size() > 1) {
                std::cerr << "dircolors: unrecognized option '" << arg << "'\n";
                return false;
            } else {
                opts.configFile = arg;
            }
        }
        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class DircolorsReporter {
public:
    static int dispatch(const std::string& content, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"ls_colors\":\"" + content + "\"}\n";
        } else if (format == 2) {
            text = "ls_colors\n\"" + content + "\"\n";
        } else if (format == 3) {
            text = "LS_COLORS\n---------\n" + content + "\n";
        } else {
            text = content;
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else {
            std::cout << text;
        }
        return 0;
    }
};

// ============================================================================
// 3. DATABASE PARSER ENGINE
// ============================================================================

class ColorDatabaseParser {
private:
    static inline const char* DEFAULT_DATABASE = R"(
# Default dircolors configuration for Windows
RESET 0
DIR 01;34
LINK 01;36
EXEC 01;32
FIFO 40;33
SOCK 01;35
BLK 40;33;01
CHR 40;33;01
ORPHAN 40;31;01
MISSING 00

# Executables & Scripts on Windows
.exe 01;32
.bat 01;32
.cmd 01;32
.ps1 01;32
.com 01;32
.msi 01;32

# Archives / Compressed files
.tar 01;31
.tgz 01;31
.zip 01;31
.z 01;31
.gz 01;31
.bz2 01;31
.xz 01;31
.7z 01;31
.rar 01;31

# Documents
.pdf 00;32
.doc 00;32
.docx 00;32
.xls 00;32
.xlsx 00;32
.ppt 00;32
.pptx 00;32

# Images & Media
.jpg 01;35
.jpeg 01;35
.png 01;35
.gif 01;35
.bmp 01;35
.svg 01;35
.mp3 00;36
.wav 00;36
.mp4 01;35
.mkv 01;35
.avi 01;35
)";

    static inline const std::unordered_map<std::string, std::string> KEY_MAP = {
        {"RESET", "rs"}, {"DIR", "di"}, {"LINK", "ln"}, {"MULTIHARDLINK", "mh"},
        {"FIFO", "fi"}, {"SOCK", "so"}, {"DOOR", "do"}, {"BLK", "bd"},
        {"CHR", "cd"}, {"ORPHAN", "or"}, {"MISSING", "mi"}, {"SETUID", "su"},
        {"SETGID", "sg"}, {"CAPABILITY", "ca"}, {"STICKY_OTHER_WRITABLE", "tw"},
        {"OTHER_WRITABLE", "ow"}, {"STICKY", "st"}, {"EXEC", "ex"}, {"FILE", "fi"}
    };

public:
    static const char* getDefaultDatabase() {
        return DEFAULT_DATABASE;
    }

    static std::string parseDatabase(std::istream& in) {
        std::string line;
        std::string lsColors;

        while (std::getline(in, line)) {
            size_t comment = line.find('#');
            if (comment != std::string::npos) line = line.substr(0, comment);

            std::istringstream iss(line);
            std::string key, val;
            if (!(iss >> key >> val)) continue;

            std::string code;
            auto it = KEY_MAP.find(key);
            if (it != KEY_MAP.end()) {
                code = it->second + "=" + val;
            } else if (!key.empty() && key[0] == '.') {
                code = "*" + key + "=" + val;
            } else {
                continue;
            }

            if (!lsColors.empty()) lsColors += ":";
            lsColors += code;
        }

        return lsColors;
    }
};

class DircolorsEngine {
private:
    DircolorsOptions options;

public:
    explicit DircolorsEngine(DircolorsOptions opts) : options(std::move(opts)) {}

    int execute() {
        if (options.shell == TargetShell::PrintDatabase) {
            std::cout << ColorDatabaseParser::getDefaultDatabase();
            return 0;
        }

        std::string lsColors;
        if (!options.configFile.empty()) {
            std::ifstream file(options.configFile);
            if (!file.is_open()) {
                std::cerr << "dircolors: cannot open '" << options.configFile << "'\n";
                return 1;
            }
            lsColors = ColorDatabaseParser::parseDatabase(file);
        } else {
            std::istringstream iss(ColorDatabaseParser::getDefaultDatabase());
            lsColors = ColorDatabaseParser::parseDatabase(iss);
        }

        std::string command;
        switch (options.shell) {
            case TargetShell::Bourne:
                command = "LS_COLORS='" + lsColors + "'; export LS_COLORS\n";
                break;
            case TargetShell::CShell:
                command = "setenv LS_COLORS '" + lsColors + "'\n";
                break;
            case TargetShell::PowerShell:
                command = "$env:LS_COLORS = \"" + lsColors + "\"\n";
                break;
            case TargetShell::Cmd:
                command = "SET LS_COLORS=" + lsColors + "\n";
                break;
            default:
                command = lsColors + "\n";
                break;
        }

        return DircolorsReporter::dispatch(command, options.outputFormat, options.pipeCommand);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class DircolorsApp {
public:
    static int run(int argc, char* argv[]) {
        DircolorsOptions options;
        if (!DircolorsOptions::parse(argc, argv, options)) {
            return 1;
        }
        DircolorsEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return DircolorsApp::run(argc, argv);
}
