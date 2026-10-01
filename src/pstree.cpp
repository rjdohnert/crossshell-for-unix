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

#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <memory>
#include <sstream>
#include <iomanip>
#include <cwctype>
#include <io.h>
#include <fcntl.h>

// ============================================================================
// CONSTANTS & VERSION INFORMATION
// ============================================================================
namespace VersionInfo {
    constexpr wchar_t VERSION[] = L"1.0.0";
    constexpr wchar_t RELEASE_DATE[] = L"2026-08-12";
    constexpr wchar_t AUTHOR[] = L"Roberto J Dohnert";
}

// ============================================================================
// DUAL-SENTINEL WIN32 RAII HANDLE WRAPPER
// ============================================================================
class ScopedHandle {
private:
    HANDLE m_handle = INVALID_HANDLE_VALUE;

public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) noexcept : m_handle(h) {}
    
    ~ScopedHandle() noexcept {
        Close();
    }

    // Disable Copying
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    // Enable Move Semantics
    ScopedHandle(ScopedHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    void Close() noexcept {
        if (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

    [[nodiscard]] HANDLE get() const noexcept { return m_handle; }
    [[nodiscard]] bool isValid() const noexcept { return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE; }
    explicit operator bool() const noexcept { return isValid(); }
};

// ============================================================================
// CONFIGURATION & TYPES
// ============================================================================
enum class TreeStyle {
    Unicode,    // UTF-16 Box Drawing Characters (├── └── │)
    ASCII       // Classic ASCII fallback (|-- `-- |)
};

struct Config {
    bool showPids        = false;
    bool showFullPaths   = false;
    bool numericSort     = false;
    bool colorOutput     = false;
    bool showParents     = false;
    DWORD targetPid      = 0;
    TreeStyle style      = TreeStyle::ASCII;
};

struct TreeSymbols {
    std::wstring branch;    // Item in middle of list
    std::wstring last;      // Last item in list
    std::wstring vertical;  // Vertical line for indentation
    std::wstring space;     // Empty padding
};

struct ProcessNode {
    DWORD pid = 0;
    DWORD ppid = 0;
    std::wstring name;
    std::wstring fullPath;
    std::vector<DWORD> children;
    bool isRootCandidate = false;
};

using ProcessMap = std::unordered_map<DWORD, ProcessNode>;

// ============================================================================
// ANSI COLOR CODES
// ============================================================================
namespace Color {
    constexpr wchar_t RESET[]       = L"\033[0m";
    constexpr wchar_t BOLD[]        = L"\033[1m";
    constexpr wchar_t RED[]         = L"\033[31m";
    constexpr wchar_t CYAN[]        = L"\033[36m";
    constexpr wchar_t BRIGHT_RED[]  = L"\033[91m";
}

// ============================================================================
// CONSOLE ENVIRONMENT SETUP
// ============================================================================
static bool EnableVirtualTerminalAndUnicode() noexcept {
    // Set stdout stream to UTF-16 wide mode
    if (_setmode(_fileno(stdout), _O_U16TEXT) == -1) {
        return false;
    }

    // Enable Virtual Terminal Processing for ANSI Escapes in Windows Console
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != NULL) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
    return true;
}

// ============================================================================
// PROCESS INFORMATION RETRIEVAL
// ============================================================================
static std::wstring QueryProcessImagePath(DWORD pid) noexcept {
    if (pid == 0 || pid == 4) {
        return L""; // System / Idle process image path unavailable
    }

    ScopedHandle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!hProcess.isValid()) {
        return L"";
    }

    wchar_t pathBuffer[MAX_PATH];
    DWORD dwSize = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess.get(), 0, pathBuffer, &dwSize)) {
        return std::wstring(pathBuffer);
    }
    return L"";
}

static bool SnapshotProcesses(ProcessMap& outMap) noexcept {
    ScopedHandle hSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!hSnapshot.isValid()) {
        return false;
    }

    PROCESSENTRY32W pe32{};
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hSnapshot.get(), &pe32)) {
        return false;
    }

    do {
        ProcessNode node;
        node.pid = pe32.th32ProcessID;
        node.ppid = pe32.th32ParentProcessID;
        node.name = pe32.szExeFile;
        outMap[node.pid] = std::move(node);
    } while (Process32NextW(hSnapshot.get(), &pe32));

    return true;
}

// ============================================================================
// COMPREHENSIVE HELP SECTION
// ============================================================================
static void PrintHelp() {
    std::wcout << LR"(pstree(1)                  CrossShell for UNIX Reference Manual                 pstree(1)

    NAME
        pstree - display a tree of processes

    SYNOPSIS
        pstree [OPTIONS] [PID]

    DESCRIPTION
        pstree displays running processes as a tree structure, illustrating
        parent-child relationships between active processes. If a PID is
        specified, the output is rooted at that process.

    OPTIONS
        -p, --show-pids
            Show process IDs (PIDs) in decimal alongside process names.

        -a, --arguments, --full-path
            Display full executable paths instead of just process names.

        -n, --numeric-sort
            Sort child processes numerically by PID instead of by name.

        -s, --show-parents
            Show ancestor processes of the specified PID up to the system root.

        -A, --ascii
            Use classic ASCII characters (|--, `--) to draw the tree.

        -U, --unicode
            Use Unicode box-drawing characters (├──, └──, │) to draw the tree.

        -c, --color
            Enable ANSI colorized output for process names and IDs.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        pstree
            Display the complete process tree in ASCII format.

        pstree -p
            Display process tree including process IDs.

        pstree -p -s 1234
            Display process 1234 and trace its ancestor chain to the root.

        pstree -A -n
            Display process tree with children sorted numerically by PID.

        pstree -U -c
            Display process tree using Unicode line drawing and ANSI colors.

    CrossShell for UNIX                                                          pstree(1)
)";
}

static void PrintVersion() {
    std::wcout << L"pstree v" << VersionInfo::VERSION 
               << L" (" << VersionInfo::RELEASE_DATE << L")\n"
               << L"Copyright (C) 2026 " << VersionInfo::AUTHOR << L"\n";
}

// ============================================================================
// CLI PARSER
// ============================================================================
static bool ParseCommandLine(int argc, wchar_t* argv[], Config& config) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
            PrintHelp();
            exit(0);
        } else if (arg == L"-v" || arg == L"--version") {
            PrintVersion();
            exit(0);
        } else if (arg == L"-p" || arg == L"--show-pids") {
            config.showPids = true;
        } else if (arg == L"-a" || arg == L"--arguments" || arg == L"--full-path") {
            config.showFullPaths = true;
        } else if (arg == L"-n" || arg == L"--numeric-sort") {
            config.numericSort = true;
        } else if (arg == L"-s" || arg == L"--show-parents") {
            config.showParents = true;
        } else if (arg == L"-A" || arg == L"--ascii") {
            config.style = TreeStyle::ASCII;
        } else if (arg == L"-U" || arg == L"--unicode") {
            config.style = TreeStyle::Unicode;
        } else if (arg == L"-c" || arg == L"--color") {
            config.colorOutput = true;
        } else if (!arg.empty() && arg[0] != L'-') {
            try {
                size_t pos = 0;
                config.targetPid = std::stoul(arg, &pos);
                if (pos != arg.length()) {
                    std::wcerr << L"Error: Invalid PID parameter '" << arg << L"'. Must be a positive integer.\n";
                    return false;
                }
            } catch (...) {
                std::wcerr << L"Error: Invalid PID parameter '" << arg << L"'. Out of integer range.\n";
                return false;
            }
        } else {
            std::wcerr << L"Error: Unknown option '" << arg << L"'. Use 'pstree --help' for usage.\n";
            return false;
        }
    }
    return true;
}

// ============================================================================
// TREE RENDERER ENGINE
// ============================================================================
class TreeRenderer {
public:
    TreeRenderer(const Config& config, ProcessMap& map) 
        : m_config(config), m_map(map) 
    {
        InitializeSymbols();
    }

    void Render() {
        BuildTreeStructure();

        if (m_config.targetPid != 0) {
            if (m_map.find(m_config.targetPid) == m_map.end()) {
                std::wcerr << L"pstree: Process ID " << m_config.targetPid << L" not found in process table.\n";
                exit(3);
            }

            if (m_config.showParents) {
                RenderWithAncestors(m_config.targetPid);
                return;
            } else {
                std::unordered_set<DWORD> visited;
                RenderNode(m_config.targetPid, L"", true, visited);
                return;
            }
        }

        // Render full system process forest
        std::unordered_set<DWORD> visited;
        std::vector<DWORD> rootPids;

        for (const auto& [pid, node] : m_map) {
            if (node.isRootCandidate) {
                rootPids.push_back(pid);
            }
        }

        SortPids(rootPids);

        for (size_t i = 0; i < rootPids.size(); ++i) {
            bool isLast = (i == rootPids.size() - 1);
            RenderNode(rootPids[i], L"", isLast, visited);
        }
    }

private:
    const Config& m_config;
    ProcessMap& m_map;
    TreeSymbols m_sym;

    void InitializeSymbols() noexcept {
        if (m_config.style == TreeStyle::ASCII) {
            m_sym.branch   = L"|-- ";
            m_sym.last     = L"`-- ";
            m_sym.vertical = L"|   ";
            m_sym.space    = L"    ";
        } else {
            // Unicode Standard Default
            m_sym.branch   = L"├── ";
            m_sym.last     = L"└── ";
            m_sym.vertical = L"│   ";
            m_sym.space    = L"    ";
        }
    }

    void BuildTreeStructure() {
        for (auto& [pid, node] : m_map) {
            if (m_config.showFullPaths) {
                node.fullPath = QueryProcessImagePath(pid);
            }

            // Mark orphan or top-level process nodes
            if (node.ppid == 0 || node.ppid == node.pid || m_map.find(node.ppid) == m_map.end()) {
                node.isRootCandidate = true;
            } else {
                m_map[node.ppid].children.push_back(pid);
            }
        }

        // Sort children for every process node
        for (auto& [pid, node] : m_map) {
            SortPids(node.children);
        }
    }

    void SortPids(std::vector<DWORD>& pids) {
        if (m_config.numericSort) {
            std::sort(pids.begin(), pids.end());
        } else {
            std::sort(pids.begin(), pids.end(), [this](DWORD a, DWORD b) {
                const auto& nameA = m_map[a].name;
                const auto& nameB = m_map[b].name;
                int cmp = _wcsicmp(nameA.c_str(), nameB.c_str());
                if (cmp != 0) return cmp < 0;
                return a < b; // Secondary tie-breaker by PID
            });
        }
    }

    void PrintNodeLabel(const ProcessNode& node) {
        std::wstring displayName = (m_config.showFullPaths && !node.fullPath.empty()) 
                                    ? node.fullPath 
                                    : node.name;
        std::transform(displayName.begin(), displayName.end(), displayName.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(std::towupper(ch));
        });

        if (m_config.colorOutput) {
            const bool isSystemProcess = (node.pid == 0 || node.pid == 4);
            const wchar_t* color = isSystemProcess ? Color::RED : Color::CYAN;
            const wchar_t* weight = isSystemProcess ? Color::BOLD : L"";

            std::wcout << weight << color << displayName;
            if (m_config.showPids) {
                std::wcout << L"(" << node.pid << L")";
            }
            std::wcout << Color::RESET;
        } else {
            std::wcout << displayName;
            if (m_config.showPids) {
                std::wcout << L"(" << node.pid << L")";
            }
        }
        std::wcout << L"\n";
    }

    void RenderNode(DWORD pid, const std::wstring& prefix, bool isLast, std::unordered_set<DWORD>& visited) {
        if (visited.count(pid)) {
            // Guard against stack overflow due to recycled PIDs
            std::wcout << prefix << (isLast ? m_sym.last : m_sym.branch) << L"[CYCLE DETECTED: PID " << pid << L"]\n";
            return;
        }
        visited.insert(pid);

        auto it = m_map.find(pid);
        if (it == m_map.end()) return;

        const auto& node = it->second;

        if (!prefix.empty()) {
            std::wcout << prefix << (isLast ? m_sym.last : m_sym.branch);
        }

        PrintNodeLabel(node);

        std::wstring childPrefix = prefix + (isLast ? m_sym.space : m_sym.vertical);
        const auto& children = node.children;

        for (size_t i = 0; i < children.size(); ++i) {
            bool childIsLast = (i == children.size() - 1);
            RenderNode(children[i], childPrefix, childIsLast, visited);
        }
    }

    void RenderWithAncestors(DWORD targetPid) {
        std::vector<DWORD> ancestors;
        DWORD current = targetPid;

        std::unordered_set<DWORD> loopCheck;
        while (current != 0 && m_map.find(current) != m_map.end()) {
            if (loopCheck.count(current)) break;
            loopCheck.insert(current);

            ancestors.push_back(current);
            DWORD parent = m_map[current].ppid;
            if (parent == current) break;
            current = parent;
        }

        std::reverse(ancestors.begin(), ancestors.end());

        std::wstring currentPrefix = L"";
        std::unordered_set<DWORD> visited;

        for (size_t i = 0; i < ancestors.size(); ++i) {
            DWORD pid = ancestors[i];
            const auto& node = m_map[pid];

            if (i > 0) {
                std::wcout << currentPrefix << m_sym.last;
            }

            PrintNodeLabel(node);

            if (pid == targetPid) {
                // Render full child subtree for target PID
                std::wstring childPrefix = currentPrefix + (i > 0 ? m_sym.space : L"");
                const auto& children = node.children;
                for (size_t c = 0; c < children.size(); ++c) {
                    bool childIsLast = (c == children.size() - 1);
                    RenderNode(children[c], childPrefix, childIsLast, visited);
                }
            } else {
                currentPrefix += m_sym.space;
            }
        }
    }
};

// ============================================================================
// MAIN PROGRAM ENTRY POINT
// ============================================================================
int wmain(int argc, wchar_t* argv[]) {
    if (!EnableVirtualTerminalAndUnicode()) {
        std::wcerr << L"Warning: Unable to initialize UTF-16 console rendering mode.\n";
    }

    Config config;
    if (!ParseCommandLine(argc, argv, config)) {
        return 1;
    }

    ProcessMap processMap;
    if (!SnapshotProcesses(processMap)) {
        std::wcerr << L"Error: Failed to obtain system process snapshot (Win32 Error: " 
                   << GetLastError() << L").\n";
        return 2;
    }

    TreeRenderer renderer(config, processMap);
    renderer.Render();

    return 0;
}