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

#include "path_helper.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

std::string PathHelper::Utf8(const std::wstring& value) {
#ifdef _WIN32
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
#else
    return std::string(value.begin(), value.end());
#endif
}

bool PathHelper::DirectoryExists(const std::wstring& path) {
#ifdef _WIN32
    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
           (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    (void)path;
    return false;
#endif
}

std::wstring PathHelper::GetAbsolutePath(const std::wstring& path) {
#ifdef _WIN32
    wchar_t fullPath[MAX_PATH];
    DWORD retval = GetFullPathNameW(path.c_str(), MAX_PATH, fullPath, NULL);
    if (retval == 0 || retval > MAX_PATH) {
        return path;
    }
    return std::wstring(fullPath);
#else
    return path;
#endif
}

std::wstring PathHelper::BuildCommandLine(const std::wstring& command, const std::vector<std::wstring>& args) {
    std::wstring commandLine = command;
    for (const auto& arg : args) {
        commandLine += L" \"" + arg + L"\"";
    }
    return commandLine;
}

bool EnvironmentConfigurator::ConfigureSandbox(const std::wstring& newRoot) {
#ifdef _WIN32
    if (!SetCurrentDirectoryW(newRoot.c_str())) {
        return false;
    }

    std::wstring winDir = newRoot + L"\\Windows";
    std::wstring sys32Dir = newRoot + L"\\Windows\\System32";
    std::wstring binDir = newRoot + L"\\bin";
    std::wstring tempDir = newRoot + L"\\temp";

    std::wstring newPath = binDir + L";" + sys32Dir + L";" + winDir + L";" + newRoot;
    SetEnvironmentVariableW(L"PATH", newPath.c_str());
    SetEnvironmentVariableW(L"SystemRoot", winDir.c_str());
    SetEnvironmentVariableW(L"USERPROFILE", newRoot.c_str());
    SetEnvironmentVariableW(L"TEMP", tempDir.c_str());
    SetEnvironmentVariableW(L"TMP", tempDir.c_str());

    if (newRoot.length() >= 2 && newRoot[1] == L':') {
        std::wstring drive = newRoot.substr(0, 2);
        SetEnvironmentVariableW(L"SystemDrive", drive.c_str());
    }

    return true;
#else
    (void)newRoot;
    return false;
#endif
}
