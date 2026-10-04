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

#include "parser.hpp"
#include <iostream>

void OptionParser::DisplayHelp() {
    std::cout << R"(awk(1)                  CrossShell for UNIX Reference Manual                          awk(1)

    NAME
        awk - pattern scanning and processing language

    SYNOPSIS
        awk [QUALIFIERS...] 'PROGRAM' [FILE...]
        awk [QUALIFIERS...] [FILE...]
        <input-stream> | awk [QUALIFIERS...] 'PROGRAM'

    DESCRIPTION
        awk parses, filters, transforms, and projects structured data streams.
        It extends classic POSIX AWK semantics to natively understand both
        delimited tabular text ($1, $2, ...) and structured objects/JSON Lines
        (.key, .nested.field, .items[0]).

    OPTIONS
        -F, --field-separator SEP
            Define input field separator regular string (default: whitespace).

        -O, --output-separator SEP
            Define output field separator (OFS) (default: single space).

        -H, --headers
            Treat the first record of CSV/TSV input as column headers. Enables
            named field lookups like $Name, $Salary, .Age.

        -j, --json-only
            Force pure JSON/Object mode. Skips positional text parsing.

        -t, --text-only
            Force pure Text mode. Disables JSON auto-detection.

        -0, --null-delimited
            Input and output records are terminated by ASCII NUL (\0).

        --registry KEY
            Read registry values as JSON Lines (for example HKLM\SOFTWARE\...).

        --wmi QUERY[|NAMESPACE]
            Read WMI objects as JSON Lines (default namespace ROOT\CIMV2).

        -i, --ignore-case
            Perform case-insensitive regular expression comparisons (~, !~).

        -c, --condition EXPR
            Filter predicate evaluated per record. If true, prints or executes.

        -p, --print TEMPLATE
            Output template interpolated per matching record.

        -B, --begin TEXT
            Text/Action executed before any input records are processed.

        -E, --end TEXT
            Text/Action executed after all records/files are processed.

        -v, --assign VAR=VALUE
            Assign a constant user variable accessible anywhere in expressions.

        -f, --file SCRIPTFILE
            Read script pattern/action parameters from a file.

        -e, --source PROGRAM
            Add program source text. May be specified more than once.

        --posix, -P
            Select POSIX compatibility mode.

        --traditional, -t
            Select traditional awk compatibility mode.

        --lint, -L
            Enable compatibility diagnostics.

        --sandbox, -S
            Enable restricted execution mode.

        --include FILE, -i FILE
            Include another awk source file.

        --load FILE, -l FILE
            Load another awk source file before processing.

        --dump-variables[=FILE]
            Request a variable dump after processing.

        --pretty-print[=FILE]
            Request formatted program output.

        --profile[=FILE]
            Request execution profiling.

        --non-decimal-data
            Enable non-decimal numeric data compatibility.

        --use-lc-numeric
            Use locale numeric conventions.

        --bignum, -M
            Enable arbitrary-precision numeric compatibility.

        --copyright, -C
            Display copyright information and exit.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and environment information and exit.

    SPECIAL BUILT-IN VARIABLES
        $0
            The entire raw line/record.

        $1 ... $N
            Positional fields in delimited text mode (1-indexed).

        $NF
            The value of the last positional field.

        $HeaderName
            Column value matching header name (with -H/--headers).

        .path.key
            Object property accessor for JSON Lines (e.g., .user.id).

        .array[index]
            Object array indexer (e.g., .records[0].name).

        NR, FNR, NF, FILENAME, FS, OFS
            Standard record counters, field count, filename, and separators.

    EXAMPLES
        awk -F: '$3 >= 1000 { print $1 }' /etc/passwd
            Print usernames with UID >= 1000.

        awk -H -p '$Name ~ /admin/ { print $0 }' data.csv
            Filter CSV rows where Name matches admin.

        awk -c '.status >= 400 && .env == "prod"' events.jsonl
            Filter JSON Lines stream for production errors.

    CrossShell for UNIX                                                    awk(1)
)";
}

void OptionParser::DisplayVersion() {
    std::cout << "awk version 2.0.0\n"
              << "Copyright (C) 2026, Roberto J. Dohnert\n";
}

void OptionParser::DisplayCopyright() {
    std::cout << "Copyright (C) 2026, Roberto J. Dohnert\n";
}

bool OptionParser::Parse(int argc, char* argv[], AwkOptions& opts, bool& exitEarly) const {
    exitEarly = false;
    std::string positional_script = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (opts.end_of_options) {
            if (positional_script.empty()) positional_script = arg;
            else opts.files.push_back(arg);
            continue;
        }
        if (arg == "--") {
            opts.end_of_options = true;
            continue;
        }

        if (arg == "-h" || arg == "--help" || arg == "-?") {
            DisplayHelp();
            exitEarly = true;
            return true;
        } else if (arg == "-V" || arg == "--version") {
            DisplayVersion();
            exitEarly = true;
            return true;
        } else if (arg == "-C" || arg == "--copyright") {
            DisplayCopyright();
            exitEarly = true;
            return true;
        } else if ((arg == "-e" || arg == "--source") && i + 1 < argc) {
            opts.source_scripts.push_back(argv[++i]);
        } else if (arg.rfind("--source=", 0) == 0) {
            opts.source_scripts.push_back(arg.substr(9));
        } else if (arg == "--posix" || arg == "-P") {
            opts.posix_mode = true;
        } else if (arg == "--traditional") {
            opts.traditional_mode = true;
        } else if (arg == "--lint" || arg == "-L") {
            opts.lint_mode = true;
        } else if (arg == "--sandbox" || arg == "-S") {
            opts.sandbox_mode = true;
        } else if (arg == "--non-decimal-data") {
            opts.non_decimal_data = true;
        } else if (arg == "--use-lc-numeric") {
            opts.use_lc_numeric = true;
        } else if (arg == "--bignum" || arg == "-M") {
            opts.bignum_mode = true;
        } else if (arg == "--include" && i + 1 < argc) {
            opts.script_files.push_back(argv[++i]);
        } else if (arg.rfind("--include=", 0) == 0) {
            opts.script_files.push_back(arg.substr(10));
        } else if (arg == "--load" && i + 1 < argc) {
            opts.script_files.push_back(argv[++i]);
        } else if (arg.rfind("--load=", 0) == 0) {
            opts.script_files.push_back(arg.substr(7));
        } else if (arg == "--dump-variables" || arg.rfind("--dump-variables=", 0) == 0) {
            opts.dump_variables = true;
        } else if (arg == "--pretty-print" || arg.rfind("--pretty-print=", 0) == 0) {
            opts.pretty_print = true;
        } else if (arg == "--profile" || arg.rfind("--profile=", 0) == 0) {
            opts.profile = true;
        } else if ((arg == "-F" || arg == "--field-separator") && i + 1 < argc) {
            opts.fs = argv[++i];
        } else if (arg.rfind("-F", 0) == 0 && arg.size() > 2) {
            opts.fs = arg.substr(2);
        } else if ((arg == "-O" || arg == "--output-separator") && i + 1 < argc) {
            opts.ofs = argv[++i];
        } else if ((arg == "-c" || arg == "--condition") && i + 1 < argc) {
            opts.condition = argv[++i];
        } else if ((arg == "-p" || arg == "--print") && i + 1 < argc) {
            opts.print_fmt = argv[++i];
        } else if ((arg == "-B" || arg == "--begin") && i + 1 < argc) {
            opts.begin_action = argv[++i];
        } else if ((arg == "-E" || arg == "--end") && i + 1 < argc) {
            opts.end_action = argv[++i];
        } else if (arg == "-H" || arg == "--headers") {
            opts.has_header = true;
        } else if (arg == "-j" || arg == "--json-only") {
            opts.json_only = true;
        } else if (arg == "-t" || arg == "--text-only") {
            opts.text_only = true;
        } else if (arg == "-i" || arg == "--ignore-case") {
            opts.ignore_case = true;
        } else if (arg == "-0" || arg == "--null-delimited") {
            opts.record_delim = '\0';
        } else if ((arg == "-v" || arg == "--assign") && i + 1 < argc) {
            std::string assign = argv[++i];
            size_t eq = assign.find('=');
            if (eq != std::string::npos) {
                opts.user_vars[assign.substr(0, eq)] = Value(assign.substr(eq + 1));
            }
        } else if ((arg == "-f" || arg == "--file") && i + 1 < argc) {
            opts.script_files.push_back(argv[++i]);
        } else if ((arg == "--registry" || arg == "--wmi") && i + 1 < argc) {
            opts.object_sources.push_back((arg == "--registry" ? "registry:" : "wmi:") + std::string(argv[++i]));
        } else if (!arg.empty() && arg[0] != '-') {
            if (opts.condition.empty() && opts.print_fmt.empty() && opts.script_files.empty() && positional_script.empty()) {
                positional_script = arg;
                int brace_count = 0;
                for (char c : positional_script) {
                    if (c == '{') brace_count++;
                    else if (c == '}') brace_count--;
                }
                while (brace_count > 0 && i + 1 < argc) {
                    std::string next_token = argv[++i];
                    positional_script += " " + next_token;
                    for (char c : next_token) {
                        if (c == '{') brace_count++;
                        else if (c == '}') brace_count--;
                    }
                }
            } else if (!positional_script.empty() && arg.find('=') != std::string::npos &&
                       arg.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789=") == std::string::npos) {
                size_t eq = arg.find('=');
                opts.user_vars[arg.substr(0, eq)] = Value(arg.substr(eq + 1));
            } else {
                opts.files.push_back(arg);
            }
        }
    }

    if (!positional_script.empty()) {
        m_scriptLoader.ParseInlineScript(positional_script, opts);
    }

    return true;
}
