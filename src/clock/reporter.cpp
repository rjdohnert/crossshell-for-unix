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

#include "reporter.hpp"
#include <iostream>
#include <cstdio>

std::string ClockReporter::escapeJson(const std::string& value) {
    std::string escaped;
    for (char ch : value) {
        if (ch == '\\' || ch == '"') {
            escaped += '\\';
        }
        escaped += ch;
    }
    return escaped;
}

int ClockReporter::dispatch(const ClockDisplay& display, int format, const std::string& pipeCommand) {
    std::string text;
    if (format == 1) {
        text = "{\"timestamp\":\"" + escapeJson(display.dateTime) +
            "\",\"timezone\":\"" + escapeJson(display.timeZone) +
            "\",\"time_mode\":\"" + escapeJson(display.timeMode) + "\"}\n";
    } else if (format == 2) {
        text = "timestamp,timezone,time_mode\n" + display.dateTime + "," +
            display.timeZone + "," + display.timeMode + "\n";
    } else if (format == 3) {
        text = "TIMESTAMP\n---------\n" + display.dateTime +
            "\nTIMEZONE\n--------\n" + display.timeZone +
            " (" + display.timeMode + ")\n";
    } else {
        text = display.dateTime + "\nTimezone: " + display.timeZone +
            " (" + display.timeMode + ")\n";
    }

    if (!pipeCommand.empty()) {
#ifdef _WIN32
        FILE* pipe = _popen(pipeCommand.c_str(), "w");
        if (!pipe) return 1;
        std::fwrite(text.data(), 1, text.size(), pipe);
        _pclose(pipe);
#else
        FILE* pipe = popen(pipeCommand.c_str(), "w");
        if (!pipe) return 1;
        std::fwrite(text.data(), 1, text.size(), pipe);
        pclose(pipe);
#endif
    } else {
        std::cout << text;
    }
    return 0;
}
