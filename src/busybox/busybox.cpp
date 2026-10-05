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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
Standardized Section Index
---------------------------
1. Platform includes, shared constants, and forward declarations
2. Runtime utilities, shell context, tokenization, and glob expansion
3. Shell AST, parser, and execution engine
4. Applet implementations and dispatch
5. Self-tests and CLI entry point
*/

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

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Crypt32.lib")

namespace fs = std::filesystem;

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

// Forward Declarations
struct ShellContext;
class ASTNode;

#include "busybox_runtime.inc"
#include "busybox_shell.inc"
#include "busybox_applets.inc"
#include "busybox_cli.inc"
