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

#ifndef CHROOT_MODELS_HPP
#define CHROOT_MODELS_HPP

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <cstdio>

enum class OutputFormat {
    Default = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

#ifdef _WIN32
class ScopedJobHandle {
public:
    explicit ScopedJobHandle(HANDLE handle = nullptr) : m_handle(handle) {}
    ~ScopedJobHandle() { Close(); }

    ScopedJobHandle(const ScopedJobHandle&) = delete;
    ScopedJobHandle& operator=(const ScopedJobHandle&) = delete;

    ScopedJobHandle(ScopedJobHandle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = nullptr; }
    ScopedJobHandle& operator=(ScopedJobHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (IsValid()) {
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }

private:
    HANDLE m_handle;
};

class ScopedProcessInfo {
public:
    PROCESS_INFORMATION pi = { 0 };

    ~ScopedProcessInfo() {
        if (pi.hProcess && pi.hProcess != INVALID_HANDLE_VALUE) {
            CloseHandle(pi.hProcess);
        }
        if (pi.hThread && pi.hThread != INVALID_HANDLE_VALUE) {
            CloseHandle(pi.hThread);
        }
    }

    ScopedProcessInfo() = default;
    ScopedProcessInfo(const ScopedProcessInfo&) = delete;
    ScopedProcessInfo& operator=(const ScopedProcessInfo&) = delete;
};

class ScopedWpOpen {
public:
    explicit ScopedWpOpen(FILE* pipe = nullptr) : m_pipe(pipe) {}
    ~ScopedWpOpen() { Close(); }

    ScopedWpOpen(const ScopedWpOpen&) = delete;
    ScopedWpOpen& operator=(const ScopedWpOpen&) = delete;

    ScopedWpOpen(ScopedWpOpen&& other) noexcept : m_pipe(other.m_pipe) { other.m_pipe = nullptr; }
    ScopedWpOpen& operator=(ScopedWpOpen&& other) noexcept {
        if (this != &other) {
            Close();
            m_pipe = other.m_pipe;
            other.m_pipe = nullptr;
        }
        return *this;
    }

    FILE* Get() const { return m_pipe; }
    bool IsValid() const { return m_pipe != nullptr; }

    void Close() {
        if (m_pipe) {
            _pclose(m_pipe);
            m_pipe = nullptr;
        }
    }

private:
    FILE* m_pipe;
};
#endif // _WIN32

#endif // CHROOT_MODELS_HPP
