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

#define _CRT_SECURE_NO_WARNINGS
#include "awk_app.hpp"
#include "win_loader.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

int AwkApplication::Run(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    AwkOptions opts;
    bool exitEarly = false;
    if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
        return 2;
    }
    if (exitEarly) {
        return 0;
    }

    for (const auto& source : opts.source_scripts) {
        m_scriptLoader.ParseInlineScript(source, opts);
    }

    for (const auto& script_file : opts.script_files) {
        if (!m_scriptLoader.LoadScriptFile(script_file, opts)) {
            std::cerr << "awk: cannot open script file '" << script_file << "'\n";
            return 2;
        }
    }

    if (!opts.begin_action.empty()) {
        std::cout << opts.begin_action << "\n";
    }

    opts.files.insert(opts.files.end(), opts.object_sources.begin(), opts.object_sources.end());
    if (opts.files.empty()) {
        opts.files.push_back("-");
    }

    long long total_records = 0;
    RecordContext ctx;

    for (const auto& fname : opts.files) {
        std::istream* stream_ptr = &std::cin;
        std::ifstream file_stream;
        std::istringstream object_stream;

        if (fname.rfind("registry:", 0) == 0 || fname.rfind("wmi:", 0) == 0) {
            std::string content, error;
            if (!WindowsObjectLoader::LoadObjectInput(fname, content, error)) {
                std::cerr << "awk: " << error << "\n";
                continue;
            }
            object_stream.str(std::move(content));
            stream_ptr = &object_stream;
        } else if (fname != "-") {
            file_stream.open(fname, std::ios::binary);
            if (!file_stream.is_open()) {
                std::cerr << "awk: fatal: cannot open file '" << fname << "'\n";
                continue;
            }
            stream_ptr = &file_stream;
        }

        std::string line;
        long long file_records = 0;
        bool header_processed = false;

        while (std::getline(*stream_ptr, line, opts.record_delim)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (opts.has_header && !header_processed) {
                ctx.header_map.clear();
                std::vector<std::string> cols;
                if (opts.fs == " ") {
                    std::istringstream iss(line);
                    std::string tok;
                    while (iss >> tok) cols.push_back(tok);
                } else {
                    size_t start = 0, end;
                    while ((end = line.find(opts.fs, start)) != std::string::npos) {
                        cols.push_back(line.substr(start, end - start));
                        start = end + opts.fs.length();
                    }
                    cols.push_back(line.substr(start));
                }
                for (size_t c = 0; c < cols.size(); ++c) {
                    ctx.header_map[cols[c]] = c + 1;
                }
                header_processed = true;
                continue;
            }

            total_records++;
            file_records++;
            ctx.load(line, opts, total_records, file_records, fname);

            if (m_evaluator.eval_condition(opts.condition, ctx)) {
                std::cout << m_evaluator.interpolate(opts.print_fmt, ctx) << "\n";
            }
        }
    }

    if (!opts.end_action.empty()) {
        std::cout << opts.end_action << "\n";
    }

    return 0;
}
