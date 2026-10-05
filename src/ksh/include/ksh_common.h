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

#ifndef CROSSSHELL_KSH_COMMON_H
#define CROSSSHELL_KSH_COMMON_H

#include <iostream>
#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <deque>
#include <map>
#include <unordered_map>
#include <set>
#include <optional>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <cmath>
#include <functional>
#include <mutex>
#include <thread>
#include <cwctype>
#include <regex>
#include <chrono>
#include <ctime>
#include <memory>
#include <windows.h>
#include <sddl.h>
#include <winternl.h>
#include <io.h>
#include <fcntl.h>
#include <conio.h>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "User32.lib")

// Hardened Resource Security & Depth Limits
constexpr size_t kDefaultHistoryLimit = 1000;
constexpr size_t kMaxCommandSubstitutionOutputBytes = 8 * 1024 * 1024;
constexpr size_t kMaxHereDocContentBytes = 8 * 1024 * 1024;
constexpr int kMaxExpansionNestingDepth = 64;
constexpr int kMaxEvalNestingDepth = 64;
constexpr int kMaxSubstitutionParseDepth = 128;
constexpr size_t kMaxRegexPatternLength = 512;
constexpr size_t kMaxRegexInputLength = 8192;
constexpr unsigned long long kMaxPatternMatchSteps = 200000;
constexpr int kMaxFunctionCallDepth = 200;
constexpr size_t kMaxScriptFileSizeBytes = 64 * 1024 * 1024;  // 64 MiB
constexpr size_t kMaxFunctionHeaderLineLength = 1024;
constexpr int kMaxArithmeticParseDepth = 200;
constexpr DWORD kDefaultProcessSubstitutionConnectTimeoutMs = 120000;
constexpr DWORD kMinProcessSubstitutionConnectTimeoutMs = 1000;
constexpr DWORD kMaxProcessSubstitutionConnectTimeoutMs = 600000;
constexpr size_t kMaxArrayIndex = 1000000;       // prevent OOM via arr[huge]=val
constexpr size_t kMaxShellVariables = 10000;     // per-session variable table cap
constexpr size_t kMaxShellFunctions = 1000;      // per-session function table cap
constexpr size_t kMaxShellAliases = 1000;        // per-session alias table cap
constexpr size_t kMaxTrapHandlers = 64;          // per-session trap-handler table cap
constexpr size_t kMaxHistoryLineBytes = 16384;   // 16 KiB per history line
constexpr size_t kMaxHistoryLoadEntries = 10000;  // cap history file load size

constexpr int kMinCustomFd = 3;
constexpr int kMaxCustomFd = 9;
constexpr size_t kCustomFdTableSize = 10;

const wchar_t k_array_at_quoted_separator = 0x1F;
const wchar_t* const kGetoptsCursorVar = L"__ksh_GETOPTS_POS";
const wchar_t* const kGetoptsOptindMirrorVar = L"__ksh_GETOPTS_OPTIND";

#endif // CROSSSHELL_KSH_COMMON_H
