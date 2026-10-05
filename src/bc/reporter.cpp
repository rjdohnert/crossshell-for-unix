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

#include "reporter.hpp"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>

std::string BcReporter::json_escape(const std::string& value) {
    std::string result;
    for (char ch : value) {
        if (ch == '\\') result += "\\\\";
        else if (ch == '"') result += "\\\"";
        else if (ch == '\n') result += "\\n";
        else if (ch == '\r') result += "\\r";
        else if (ch == '\t') result += "\\t";
        else result += ch;
    }
    return result;
}

std::string BcReporter::csv_escape(const std::string& value) {
    std::string result = "\"";
    for (char ch : value) {
        if (ch == '\"') result += "\"\"";
        else result += ch;
    }
    result += '\"';
    return result;
}

void BcReporter::emit_result(const std::string& expression, double result, const BcOptions& options) {
    std::ostringstream value;
    value << result;
    std::string output;
    if (options.output_format == 1) {
        output = "{\"expression\":\"" + json_escape(expression) + "\",\"result\":" + value.str() + "}\n";
    } else if (options.output_format == 2) {
        output = "expression,result\n" + csv_escape(expression) + "," + value.str() + "\n";
    } else if (options.output_format == 3) {
        output = "EXPRESSION\tRESULT\n" + expression + "\t" + value.str() + "\n";
    } else {
        output = value.str() + "\n";
    }

    if (!options.pipe_command.empty()) {
#ifdef _WIN32
        FILE* pipe = _popen(options.pipe_command.c_str(), "w");
        if (!pipe) throw std::runtime_error("cannot open pipe command");
        fwrite(output.data(), 1, output.size(), pipe);
        _pclose(pipe);
#else
        FILE* pipe = popen(options.pipe_command.c_str(), "w");
        if (!pipe) throw std::runtime_error("cannot open pipe command");
        fwrite(output.data(), 1, output.size(), pipe);
        pclose(pipe);
#endif
    } else {
        std::cout << output;
    }
}
