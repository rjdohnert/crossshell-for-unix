/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>

#pragma comment(lib, "Advapi32.lib")

namespace fs = std::filesystem;

// ============================================================================
// 1. PIPE PATH FORMATTER & SECURITY DESCRIPTOR
// ============================================================================

class PipePathFormatter {
public:
    static std::wstring FormatPipePath(const std::wstring& inputPath) {
        if (inputPath.rfind(L"\\\\.\\pipe\\", 0) == 0 || inputPath.rfind(L"//./pipe/", 0) == 0) {
            return inputPath;
        }

        fs::path p(inputPath);
        std::wstring pipeName = p.filename().wstring();
        if (pipeName.empty()) {
            pipeName = L"default_fifo";
        }

        return L"\\\\.\\pipe\\" + pipeName;
    }
};

class PipeSecurityDescriptor {
private:
    std::vector<BYTE> m_sdBuffer;
    SECURITY_ATTRIBUTES m_sa{ sizeof(SECURITY_ATTRIBUTES), nullptr, FALSE };

public:
    explicit PipeSecurityDescriptor(int mode) {
        std::wstring sddl;
        if ((mode & 0077) == 0) {
            sddl = L"D:P(A;;GA;;;OW)";
        } else {
            sddl = L"D:P(A;;GA;;;WD)";
        }

        PSECURITY_DESCRIPTOR pSD = nullptr;
        if (ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &pSD, nullptr)) {
            DWORD len = GetSecurityDescriptorLength(pSD);
            m_sdBuffer.resize(len);
            memcpy(m_sdBuffer.data(), pSD, len);
            LocalFree(pSD);
            m_sa.lpSecurityDescriptor = m_sdBuffer.data();
        }
    }

    SECURITY_ATTRIBUTES* GetSA() {
        return &m_sa;
    }
};

// ============================================================================
// 2. NAMED PIPE ENGINE
// ============================================================================

class NamedPipeEngine {
public:
    static bool CreateAndManagePipe(const std::wstring& inputPath, int mode, bool keepOpen) {
        std::wstring pipePath = PipePathFormatter::FormatPipePath(inputPath);
        PipeSecurityDescriptor sd(mode);

        HANDLE hPipe = CreateNamedPipeW(
            pipePath.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            4096,
            4096,
            0,
            sd.GetSA()
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            std::wcerr << L"mkfifo: failed to create fifo '" << pipePath
                      << L"' (Error Code: " << err << L")\n";
            return false;
        }

        std::wcout << L"mkfifo: created named pipe at " << pipePath << L"\n";

        if (keepOpen) {
            std::wcout << L"Pipe active. Waiting for client connections (Press Ctrl+C to stop)...\n";
            while (true) {
                BOOL connected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
                if (connected) {
                    DisconnectNamedPipe(hPipe);
                }
            }
        }

        CloseHandle(hPipe);
        return true;
    }
};

// ============================================================================
// 3. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <iomanip>
#include <sstream>
#include <csignal>
#include <atomic>

// Global cancellation flag for persistent FIFO handles
std::atomic<bool> g_running{true};

void signalHandler(int) {
    g_running = false;
}

// ============================================================================
// Class: WindowsPipeMode
// Encapsulates authentic Windows Named Pipe Flags (no POSIX emulation).
// ============================================================================
class WindowsPipeMode {
public:
    enum class Direction {
        Duplex,   // PIPE_ACCESS_DUPLEX (Bi-directional)
        Inbound,  // PIPE_ACCESS_INBOUND (Client to Server)
        Outbound  // PIPE_ACCESS_OUTBOUND (Server to Client)
    };

    enum class Type {
        Byte,     // PIPE_TYPE_BYTE | PIPE_READMODE_BYTE
        Message   // PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE
    };

    Direction direction = Direction::Duplex;
    Type type = Type::Byte;
    std::string sddlString = "D:(A;;GRGW;;;WD)"; // Default: Everyone (WD) Read/Write

    DWORD getOpenMode() const {
        DWORD mode = FILE_FLAG_FIRST_PIPE_INSTANCE;
        switch (direction) {
            case Direction::Inbound:  mode |= PIPE_ACCESS_INBOUND; break;
            case Direction::Outbound: mode |= PIPE_ACCESS_OUTBOUND; break;
            case Direction::Duplex:
            default:                  mode |= PIPE_ACCESS_DUPLEX; break;
        }
        return mode;
    }

    DWORD getPipeMode() const {
        DWORD mode = PIPE_WAIT;
        switch (type) {
            case Type::Message: mode |= (PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE); break;
            case Type::Byte:
            default:            mode |= (PIPE_TYPE_BYTE | PIPE_READMODE_BYTE); break;
        }
        return mode;
    }

    std::string getDirectionString() const {
        switch (direction) {
            case Direction::Inbound:  return "PIPE_ACCESS_INBOUND (0x00000001)";
            case Direction::Outbound: return "PIPE_ACCESS_OUTBOUND (0x00000002)";
            case Direction::Duplex:   return "PIPE_ACCESS_DUPLEX (0x00000003)";
        }
        return "UNKNOWN";
    }

    std::string getTypeString() const {
        switch (type) {
            case Type::Message: return "PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE (0x00000006)";
            case Type::Byte:    return "PIPE_TYPE_BYTE | PIPE_READMODE_BYTE (0x00000000)";
        }
        return "UNKNOWN";
    }
};

// ============================================================================
// Class: FifoOptions
// Command-line parameters mapped to HP-UX flags and Windows extensions.
// ============================================================================
class FifoOptions {
public:
    WindowsPipeMode mode;
    bool persist = false;       // -p: Hold handle open in NPFS until canceled
    bool verbose = false;       // -v: Detailed Windows pipe attributes
    uint32_t bufferSize = 8192; // Default 8KB buffer
    std::vector<std::string> pipeNames;
};

// ============================================================================
// Class: SecurityDescriptorManager
// Allocates and manages native Windows NT SECURITY_ATTRIBUTES.
// ============================================================================
class SecurityDescriptorManager {
private:
    PSECURITY_DESCRIPTOR pSD = nullptr;
    SECURITY_ATTRIBUTES sa;

public:
    explicit SecurityDescriptorManager(const std::string& sddl) {
        ZeroMemory(&sa, sizeof(sa));
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = FALSE;

        if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(
                sddl.c_str(), SDDL_REVISION_1, &pSD, nullptr)) {
            std::cerr << "mkfifo: warning: invalid SDDL string '" << sddl 
                      << "', falling back to default security.\n";
            sa.lpSecurityDescriptor = nullptr;
        } else {
            sa.lpSecurityDescriptor = pSD;
        }
    }

    ~SecurityDescriptorManager() {
        if (pSD) {
            LocalFree(pSD);
        }
    }

    LPSECURITY_ATTRIBUTES getAttributes() {
        return &sa;
    }
};

// ============================================================================
// Class: NamedPipeInstance
// Represents a single Windows NPFS FIFO node.
// ============================================================================
class NamedPipeInstance {
private:
    std::string fullPath;
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    WindowsPipeMode mode;
    uint32_t bufSize;

    static std::string normalizeName(const std::string& input) {
        if (input.rfind(R"(\\.\pipe\)", 0) == 0) {
            return input;
        }
        return R"(\\.\pipe\)" + input;
    }

public:
    NamedPipeInstance(const std::string& name, const WindowsPipeMode& m, uint32_t bSize)
        : fullPath(normalizeName(name)), mode(m), bufSize(bSize) {}

    ~NamedPipeInstance() {
        close();
    }

    bool create(SecurityDescriptorManager& secMgr, bool verbose) {
        hPipe = CreateNamedPipeA(
            fullPath.c_str(),
            mode.getOpenMode(),
            mode.getPipeMode(),
            PIPE_UNLIMITED_INSTANCES,
            bufSize,
            bufSize,
            0,
            secMgr.getAttributes()
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            std::cerr << "mkfifo: cannot create FIFO '" << fullPath << "': ";
            if (err == ERROR_ALREADY_EXISTS || err == ERROR_PIPE_BUSY) {
                std::cerr << "File exists (Pipe already active in NPFS)\n";
            } else if (err == ERROR_ACCESS_DENIED) {
                std::cerr << "Access denied\n";
            } else {
                std::cerr << "System Error " << err << "\n";
            }
            return false;
        }

        if (verbose) {
            std::cout << "FIFO Special Object Created in Windows NPFS:\n"
                      << "  Path:        " << fullPath << "\n"
                      << "  Access Mode: " << mode.getDirectionString() << "\n"
                      << "  Pipe Type:   " << mode.getTypeString() << "\n"
                      << "  Buffer Size: " << bufSize << " bytes\n"
                      << "  DACL (SDDL): " << mode.sddlString << "\n";
        }
        return true;
    }

    void close() {
        if (hPipe != INVALID_HANDLE_VALUE) {
            CloseHandle(hPipe);
            hPipe = INVALID_HANDLE_VALUE;
        }
    }

    const std::string& getPath() const { return fullPath; }
};

// ============================================================================
// Class: HelpFormatter
// Generates the comprehensive HP-UX Reference Manual page.
// ============================================================================
class HelpFormatter {
public:
    static void printHelp() {
        std::cout << R"~(mkfifo(1)                      Reference Manual                     mkfifo(1)

NAME
     mkfifo - make FIFO special files

SYNOPSIS
     mkfifo [-m sddl_dacl] [-t byte|message] [-d duplex|in|out] [-p] [-v] name ...
     mkfifo [-void printHelp() {
        std::wcout << LR"(mkfifo(1)               CrossShell for UNIX Reference Manual           mkfifo(1)

    NAME
        mkfifo - Creates FIFO special files as Windows named pipes in the NPFS namespace. This is a Windows-native implementation, not a POSIX emulation layer.

    SYNOPSIS
        mkfifo [OPTIONS] [ARGUMENTS...]

    DESCRIPTION
        Creates FIFO special files as Windows named pipes in the NPFS namespace. This is a Windows-native implementation, not a POSIX emulation layer.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -h, --help
            Display this reference manual.

        -v, --version
            Display version information.

    EXAMPLES
        mkfifo
            Execute mkfifo with default options.

    CrossShell for UNIX                                                      mkfifo(1)
    )";
    }elp]

DESCRIPTION
     The mkfifo command creates FIFO special files (named pipes) in the order
     specified. On Windows NT architectures, named pipes reside within the 
     Named Pipe Filesystem (NPFS) under the namespace '\\.\pipe\name'.

     Unlike UNIX filesystems, Windows deletes NPFS endpoints as soon as all
     open handles are closed. To keep the FIFO alive for other terminal sessions
     or background processes, specify the -p (--persist) option.

WINDOWS MODES
     Standard UNIX octal permission masks (such as 0666 or 0644) are not 
     synthesized. Instead, authentic Windows security and IPC modes are 
     configured directly:

     Pipe Direction (-d):
          duplex    Bi-directional communication (PIPE_ACCESS_DUPLEX) [Default]
          in        Client-to-server inbound (PIPE_ACCESS_INBOUND)
          out       Server-to-client outbound (PIPE_ACCESS_OUTBOUND)

     Transmission Mode (-t):
          byte      Continuous raw byte stream (PIPE_TYPE_BYTE) [Default]
          message   Discrete message frames (PIPE_TYPE_MESSAGE)

     Security DACL (-m):
          Configured using genuine Windows Security Descriptor Definition 
          Language (SDDL). Default is 'D:(A;;GRGW;;;WD)' (Everyone: Read/Write).

OPTIONS
     -m mode   Specifies the Windows DACL in SDDL format.
               Common examples:
                 "D:(A;;GRGW;;;WD)"        - Everyone Read/Write [Default]
                 "D:(A;;GA;;;BA)"          - Built-in Administrators Full Access
                 "D:(A;;GRGW;;;AU)(A;;GA;;;BA)" - Auth Users RW, Admins Full

     -d dir    Sets pipe direction: duplex, in, or out.

     -t type   Sets transmission type: byte or message.

     -p        (Persist) Keeps server handle(s) open in NPFS until Ctrl+C is 
               issued, ensuring the FIFO is discoverable by other processes.

     -v        (Verbose) Prints underlying Win32 pipe attributes upon creation.

     --help    Displays this reference manual page and exits.

EXAMPLES
     mkfifo my_pipe
     mkfifo -p -v /tmp/app_fifo
     mkfifo -t message -m "D:(A;;GA;;;BA)" \\.\pipe\secure_fifo
     mkfifo -d in -p logger_fifo

CrossShell for UNIX                                              mkfifo(1)
)~";
    }
};

// ============================================================================
// Class: ArgumentParser
// Parses command-line options and HP-UX syntax.
// ============================================================================
class ArgumentParser {
public:
    static bool parse(int argc, char* argv[], FifoOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h") {
                HelpFormatter::printHelp();
                exit(0);
            }
            if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
                continue;
            }
            if (arg == "-p" || arg == "--persist") {
                opts.persist = true;
                continue;
            }
            if (arg == "-m" && i + 1 < argc) {
                opts.mode.sddlString = argv[++i];
                continue;
            }
            if (arg == "-t" && i + 1 < argc) {
                std::string t = argv[++i];
                if (t == "message") opts.mode.type = WindowsPipeMode::Type::Message;
                else if (t == "byte") opts.mode.type = WindowsPipeMode::Type::Byte;
                else {
                    std::cerr << "mkfifo: invalid transmission type '" << t << "' (use byte or message)\n";
                    return false;
                }
                continue;
            }
            if (arg == "-d" && i + 1 < argc) {
                std::string d = argv[++i];
                if (d == "in") opts.mode.direction = WindowsPipeMode::Direction::Inbound;
                else if (d == "out") opts.mode.direction = WindowsPipeMode::Direction::Outbound;
                else if (d == "duplex") opts.mode.direction = WindowsPipeMode::Direction::Duplex;
                else {
                    std::cerr << "mkfifo: invalid direction '" << d << "' (use duplex, in, or out)\n";
                    return false;
                }
                continue;
            }

            if (arg.rfind("-", 0) == 0) {
                std::cerr << "mkfifo: illegal option -- " << arg << "\n"
                          << "usage: mkfifo [-m sddl] [-t byte|message] [-d duplex|in|out] [-p] [-v] name ...\n";
                return false;
            }

            opts.pipeNames.push_back(arg);
        }

        if (opts.pipeNames.empty()) {
            std::cerr << "mkfifo: missing operand\nTry 'mkfifo --help' for more information.\n";
            return false;
        }

        return true;
    }
};

// ============================================================================
// Class: FifoEngine
// Coordinates creation and lifecycle management of FIFO instances.
// ============================================================================
class FifoEngine {
private:
    FifoOptions options;

public:
    explicit FifoEngine(const FifoOptions& opts) : options(opts) {}

    int execute() {
        SecurityDescriptorManager secMgr(options.mode.sddlString);
        std::vector<std::unique_ptr<NamedPipeInstance>> pipes;

        for (const auto& name : options.pipeNames) {
            auto pipe = std::make_unique<NamedPipeInstance>(name, options.mode, options.bufferSize);
            if (!pipe->create(secMgr, options.verbose)) {
                return 1;
            }
            pipes.push_back(std::move(pipe));
        }

        if (options.persist) {
            std::signal(SIGINT, signalHandler);
            std::signal(SIGTERM, signalHandler);

            std::cout << "mkfifo: " << pipes.size() << " FIFO pipe(s) active in NPFS. Press Ctrl+C to release.\n";
            while (g_running) {
                Sleep(200);
            }
            std::cout << "\nmkfifo: releasing FIFO handles and closing endpoints.\n";
        }

        return 0;
    }
};

// ============================================================================
// Main Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    FifoOptions options;
    if (!ArgumentParser::parse(argc, argv, options)) {
        return 1;
    }

    FifoEngine engine(options);
    return engine.execute();
}