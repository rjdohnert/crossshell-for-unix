/*
 * Copyright (c) 2025, R. J. Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "options.hpp"
#include <iostream>
#include <cwctype>

namespace dig {

std::wstring DigOptions::ToUpperCopy(const std::wstring& text) {
    std::wstring out;
    out.reserve(text.size());
    for (wchar_t ch : text) {
        out.push_back(static_cast<wchar_t>(std::towupper(ch)));
    }
    return out;
}

bool DigOptions::ParseQueryType(const std::wstring& text, WORD& outType, std::wstring& outTypeName) {
    std::wstring t = ToUpperCopy(text);
    if (t == L"A") { outType = DNS_TYPE_A; outTypeName = t; return true; }
    if (t == L"AAAA") { outType = DNS_TYPE_AAAA; outTypeName = t; return true; }
    if (t == L"MX") { outType = DNS_TYPE_MX; outTypeName = t; return true; }
    if (t == L"NS") { outType = DNS_TYPE_NS; outTypeName = t; return true; }
    if (t == L"TXT") { outType = DNS_TYPE_TEXT; outTypeName = t; return true; }
    if (t == L"CNAME") { outType = DNS_TYPE_CNAME; outTypeName = t; return true; }
    if (t == L"PTR") { outType = DNS_TYPE_PTR; outTypeName = t; return true; }
    return false;
}

bool DigOptions::Parse(int argc, wchar_t* argv[]) {
    bool firstPositional = true;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"--json") { format = OutputFormat::Json; continue; }
        if (arg == L"--csv") { format = OutputFormat::Csv; continue; }
        if (arg == L"--table") { format = OutputFormat::Table; continue; }
        if (arg == L"--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
        if (arg == L"-h" || arg == L"--help") {
            showHelp = true;
            return true;
        }
        if (arg == L"--version" || arg == L"-v" || arg == L"-V") {
            showVersion = true;
            return true;
        }
        if (arg == L"+short") {
            shortOutput = true;
            continue;
        }
        if (!arg.empty() && arg[0] == L'@') {
            std::wcerr << L"dig: custom DNS server syntax (@server) is not supported in this build\n";
            return false;
        }

        WORD parsedType = DNS_TYPE_A;
        std::wstring parsedTypeName;
        if (ParseQueryType(arg, parsedType, parsedTypeName)) {
            queryType = parsedType;
            queryTypeName = parsedTypeName;
            continue;
        }

        if (firstPositional) {
            name = arg;
            firstPositional = false;
            continue;
        }

        std::wcerr << L"dig: unexpected argument: " << arg << L"\n";
        return false;
    }

    if (name.empty() && !showHelp && !showVersion) {
        std::wcerr << L"dig: missing query name\n";
        return false;
    }

    return true;
}

void DigOptions::PrintUsage(const wchar_t* programName) const {
    (void)programName;
    std::wcout << LR"(dig(1)                  CrossShell for UNIX Reference Manual                   dig(1)

    NAME
        dig - DNS lookup utility and query inspector

    SYNOPSIS
        dig [OPTIONS] [@SERVER] NAME [TYPE]

    DESCRIPTION
        dig (domain information groper) is a flexible tool for interrogating DNS
        name servers. It performs DNS lookups and displays the answers returned
        from the queried name server(s).

    OPTIONS
        @SERVER
            Query the specified DNS server directly.

        +short
            Provide terse output.

        +trace
            Trace DNS delegation from root servers.

        --json, --csv, --table
            Emit DNS records formatted as JSON, CSV, or tabular report.

        --pipe COMMAND
            Forward output to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        dig example.com A
            Lookup IPv4 address records for example.com.

        dig @8.8.8.8 github.com MX --json
            Query Google DNS for MX records in JSON format.

    CrossShell for UNIX                                                    dig(1)
)";
}

void DigOptions::PrintVersion() const {
    std::wcout << L"dig 1.0.0\n";
}

} // namespace dig
