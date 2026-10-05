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

#ifndef DD_MODELS_HPP
#define DD_MODELS_HPP

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <string>
#include <chrono>
#include <cstdint>

#ifdef _WIN32
class ScopedHandle {
private:
    HANDLE m_handle = INVALID_HANDLE_VALUE;
    bool m_is_standard = false;

public:
    ScopedHandle() = default;

    ScopedHandle(HANDLE h, bool is_standard = false)
        : m_handle(h), m_is_standard(is_standard) {}

    ~ScopedHandle() {
        Close();
    }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept
        : m_handle(other.m_handle), m_is_standard(other.m_is_standard) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            m_is_standard = other.m_is_standard;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    void Reset(HANDLE h, bool is_standard = false) {
        Close();
        m_handle = h;
        m_is_standard = is_standard;
    }

    void Close() {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr && !m_is_standard) {
            CloseHandle(m_handle);
        }
        m_handle = INVALID_HANDLE_VALUE;
    }

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr; }
};
#endif // _WIN32

enum class StatusLevel {
    DEFAULT,
    NONE,
    NOXFER,
    PROGRESS
};

struct DdOptions {
    std::string if_path;
    std::string of_path;
    uint64_t ibs = 512;
    uint64_t obs = 512;
    uint64_t count = UINT64_MAX;
    uint64_t skip = 0;
    uint64_t seek = 0;
    StatusLevel status = StatusLevel::DEFAULT;

    bool conv_ucase = false;
    bool conv_lcase = false;
    bool conv_swab = false;
    bool conv_sync = false;
    bool conv_noerror = false;
    bool conv_notrunc = false;
    bool conv_excl = false;
    int output_format = 0;
    std::string pipe_command;
};

struct TransferStats {
    uint64_t rec_in_f = 0;
    uint64_t rec_in_p = 0;
    uint64_t rec_out_f = 0;
    uint64_t rec_out_p = 0;
    uint64_t total_bytes = 0;
    std::chrono::high_resolution_clock::time_point start_time;
};

#endif // DD_MODELS_HPP
