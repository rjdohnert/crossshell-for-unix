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
 *
 * CrossShell for UNIX
 */

#include "pipeline.hpp"
#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

bool PipelineManager::isInputPiped() {
#ifdef _WIN32
    if (_isatty(_fileno(stdin)) != 0) return false;
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == nullptr || hStdin == INVALID_HANDLE_VALUE) return false;
    DWORD fileType = GetFileType(hStdin);
    if ((fileType & ~FILE_TYPE_REMOTE) == FILE_TYPE_PIPE) {
        DWORD bytesAvail = 0;
        if (PeekNamedPipe(hStdin, nullptr, 0, nullptr, &bytesAvail, nullptr)) {
            return bytesAvail > 0;
        }
    } else if ((fileType & ~FILE_TYPE_REMOTE) == FILE_TYPE_DISK) {
        return true;
    }
    return false;
#else
    return isatty(fileno(stdin)) == 0;
#endif
}

std::vector<std::wstring> PipelineManager::readPipedPaths() {
    std::vector<std::wstring> paths;
    std::wstring token;
    while (std::wcin >> token) {
        if (!token.empty()) {
            paths.push_back(token);
        }
    }
    return paths;
}
