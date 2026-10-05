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

#include "engine.hpp"
#include "reporter.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

std::string TimeFormatter::formatDateTime(const std::tm& timeInfo, const ClockOptions& opts) {
    if (opts.rfc2822) {
        char buf[128];
        if (std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %z", &timeInfo) != 0) {
            return std::string(buf);
        }
    }

    if (!opts.customFormat.empty()) {
        char buf[256];
        if (std::strftime(buf, sizeof(buf), opts.customFormat.c_str(), &timeInfo) != 0) {
            return std::string(buf);
        }
    }

    std::ostringstream ss;
    ss << std::put_time(&timeInfo, "%A, %B %d, %Y %H:%M:%S");
    return ss.str();
}

std::string TimeFormatter::wideToUtf8(const wchar_t* value) {
    if (value == nullptr || *value == L'\0') {
        return "";
    }
#ifdef _WIN32
    int length = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) {
        return "";
    }
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, &result[0], length, nullptr, nullptr);
    result.resize(static_cast<size_t>(length - 1));
    return result;
#else
    std::wstring ws(value);
    return std::string(ws.begin(), ws.end());
#endif
}

void TimeFormatter::addTimeZoneInfo(ClockDisplay& display, const ClockOptions& opts) {
    if (opts.utc) {
        display.timeZone = "UTC";
        display.timeMode = "Standard Time";
        return;
    }

#ifdef _WIN32
    TIME_ZONE_INFORMATION timeZoneInfo{};
    const DWORD timeZoneState = GetTimeZoneInformation(&timeZoneInfo);
    if (timeZoneState == TIME_ZONE_ID_DAYLIGHT) {
        display.timeZone = wideToUtf8(timeZoneInfo.DaylightName);
        display.timeMode = "Daylight Saving Time";
    } else if (timeZoneState == TIME_ZONE_ID_STANDARD) {
        display.timeZone = wideToUtf8(timeZoneInfo.StandardName);
        display.timeMode = "Standard Time";
    } else {
        display.timeZone = "Local Time";
        display.timeMode = "Standard Time";
    }
#else
    display.timeZone = "Local Time";
    display.timeMode = "Standard Time";
#endif

    if (display.timeZone.empty()) {
        display.timeZone = "Local Time";
    }
}

ClockDisplay TimeFormatter::format(const std::tm& timeInfo, const ClockOptions& opts) {
    ClockDisplay display;
    display.dateTime = formatDateTime(timeInfo, opts);
    addTimeZoneInfo(display, opts);
    return display;
}

ClockEngine::ClockEngine(ClockOptions opts) : options(std::move(opts)) {}

int ClockEngine::execute() {
    auto now = std::chrono::system_clock::now();
    std::time_t nowTimeT = std::chrono::system_clock::to_time_t(now);
    std::tm timeInfo{};

#if defined(_MSC_VER)
    if (options.utc) {
        if (gmtime_s(&timeInfo, &nowTimeT) != 0) {
            std::cerr << "clock: error retrieving UTC time.\n";
            return 1;
        }
    } else if (localtime_s(&timeInfo, &nowTimeT) != 0) {
        std::cerr << "clock: error retrieving local time.\n";
        return 1;
    }
#else
    std::tm* tmPtr = options.utc ? std::gmtime(&nowTimeT) : std::localtime(&nowTimeT);
    if (tmPtr == nullptr) {
        std::cerr << "clock: error retrieving time.\n";
        return 1;
    }
    timeInfo = *tmPtr;
#endif

    ClockDisplay display = TimeFormatter::format(timeInfo, options);
    return ClockReporter::dispatch(display, options.outputFormat, options.pipeCommand);
}
