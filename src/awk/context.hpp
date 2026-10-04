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

#ifndef AWK_CONTEXT_HPP
#define AWK_CONTEXT_HPP

#include "value.hpp"
#include <map>
#include <string>
#include <vector>

struct AwkOptions {
    std::string fs = " ";                   // Field separator
    std::string ofs = " ";                  // Output field separator
    bool has_header = false;                // -H Header mode
    bool json_only = false;                 // -j
    bool text_only = false;                 // -t
    bool ignore_case = false;               // -i
    char record_delim = '\n';               // -0 null delimited
    std::string condition = "";             // -c
    std::string print_fmt = "";             // -p
    std::string begin_action = "";          // -B
    std::string end_action = "";            // -E
    std::map<std::string, Value> user_vars; // -v
    std::vector<std::string> source_scripts;// -e / --source
    bool posix_mode = false;
    bool traditional_mode = false;
    bool lint_mode = false;
    bool sandbox_mode = false;
    bool non_decimal_data = false;
    bool use_lc_numeric = false;
    bool bignum_mode = false;
    bool show_copyright = false;
    bool dump_variables = false;
    bool pretty_print = false;
    bool profile = false;
    bool end_of_options = false;
    std::vector<std::string> files;
    std::vector<std::string> object_sources;
    std::vector<std::string> script_files;
};

class ScriptLoader {
public:
    [[nodiscard]] std::string CleanLiteral(const std::string& raw) const;
    void ParseInlineScript(const std::string& script_text, AwkOptions& options) const;
    [[nodiscard]] bool LoadScriptFile(const std::string& path, AwkOptions& options) const;
};

struct RecordContext {
    long long NR = 0;                       // Record Number across all files
    long long FNR = 0;                      // Record Number in current file
    long long NF = 0;                       // Number of Fields
    std::string FILENAME = "-";
    std::string raw_line;
    std::vector<std::string> fields;        // $0, $1, $2...
    std::map<std::string, size_t> header_map;
    Value parsed_obj;
    bool is_object = false;
    const AwkOptions* opts = nullptr;

    void load(const std::string& line, const AwkOptions& options, long long rec_nr, long long rec_fnr, const std::string& fname);
    [[nodiscard]] Value resolve(const std::string& raw_token) const;
};

#endif // AWK_CONTEXT_HPP
