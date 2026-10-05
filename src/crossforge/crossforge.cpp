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
#include <psapi.h>
#include <iostream>
#include <vector>
#include <deque>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <cctype>
#include <thread>
#include <atomic>
#include <mutex>

#pragma comment(lib, "User32.lib")

namespace fs = std::filesystem;

static std::string ReadEnvironmentVariableString(const char* name) {
    if (!name || *name == '\0') {
        return "";
    }

    DWORD needed = GetEnvironmentVariableA(name, NULL, 0);
    if (needed == 0) {
        return "";
    }

    std::vector<char> buffer(static_cast<size_t>(needed));
    DWORD written = GetEnvironmentVariableA(name, buffer.data(), needed);
    if (written == 0 || written >= needed) {
        return "";
    }

    return std::string(buffer.data(), written);
}

// Converts a UTF-8 / ANSI narrow string to a wide string for Win32 wide APIs.
static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (needed <= 0) return L"";
    std::wstring out(static_cast<size_t>(needed), L'\0');
    int written = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), needed);
    if (written <= 0) return L"";
    out.resize(static_cast<size_t>(written));
    return out;
}

// ============================================================================
// CONSTANTS & ENUMS
// ============================================================================

const std::string APP_TITLE = "CrossForge IDE v3.0.2";
const std::string COPYRIGHT_NOTICE = "Copyright (C) 2026, Roberto J. Dohnert";

#include "crossforge_module_01_supportedlanguage.inc"
#include "crossforge_module_02_themepalette.inc"
#include "crossforge_module_03_thememanager.inc"
#include "crossforge_module_04_languageconfig.inc"
#include "crossforge_module_05_languageregistry.inc"
#include "crossforge_module_06_compilerdetector.inc"
#include "crossforge_module_07_detectedtool.inc"
#include "crossforge_module_08_sandboxedexecutor.inc"
#include "crossforge_module_09_highlightstate.inc"
#include "crossforge_module_10_document.inc"
#include "crossforge_module_11_linebuffer.inc"
#include "crossforge_module_12_snapshot.inc"
#include "crossforge_module_13_crossforgeengine.inc"
#include "crossforge_module_14_menuentry.inc"
#include "crossforge_module_15_menucommand.inc"
#include "crossforge_module_16_topmenuaction.inc"
#include "crossforge_module_17_diagnostic.inc"
#include "crossforge_module_18_windowlayout.inc"
