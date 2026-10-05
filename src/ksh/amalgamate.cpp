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

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

static std::string trim(const std::string& str) {
    const size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

static bool ends_with(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static void process_file(
    const fs::path& file_path,
    bool is_header,
    std::ostream& out) {
    std::ifstream in(file_path);
    if (!in.is_open()) {
        std::cerr << "Error: unable to open file " << file_path.string() << "\n";
        return;
    }

    std::string line;
    bool in_license = false;
    bool past_license = false;
    bool first_line = true;

    while (std::getline(in, line)) {
        if (first_line) {
            first_line = false;
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line = line.substr(3);
            }
        }
        std::string trimmed = trim(line);

        // Strip leading BSD license comment
        if (!past_license) {
            if (trimmed == "/*" || starts_with(trimmed, "/*")) {
                in_license = true;
            }
            if (in_license) {
                if (trimmed == "*/" || ends_with(trimmed, "*/")) {
                    in_license = false;
                    past_license = true;
                }
                continue;
            }
            if (trimmed.empty()) {
                continue;
            }
            past_license = true;
        }

        // Strip internal ksh header includes
        if (trimmed.find("ksh_") != std::string::npos && (starts_with(trimmed, "#include") || starts_with(trimmed, "# include"))) {
            continue;
        }

        // Strip header guards
        if (is_header) {
            if (starts_with(trimmed, "#ifndef CROSSSHELL_KSH_") || starts_with(trimmed, "# define CROSSSHELL_KSH_") || starts_with(trimmed, "#define CROSSSHELL_KSH_")) {
                continue;
            }
            if (starts_with(trimmed, "#endif // CROSSSHELL_KSH_") || starts_with(trimmed, "#endif // !CROSSSHELL_KSH_")) {
                continue;
            }
        }

        // For source files, strip system includes and pragma comments (already in ksh_common.h)
        if (!is_header) {
            if (starts_with(trimmed, "#include <") || starts_with(trimmed, "# include <")) {
                continue;
            }
            if (starts_with(trimmed, "#pragma comment(lib,")) {
                continue;
            }
        }

        out << line << "\n";
    }
}

int main(int argc, char* argv[]) {
    fs::path source_dir = ".";
    fs::path output_file = "ksh.cpp";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_file = argv[++i];
        } else if ((arg == "-s" || arg == "--source-dir") && i + 1 < argc) {
            source_dir = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: amalgamate [--source-dir <dir>] [--output <file>]\n";
            return 0;
        }
    }

    fs::path include_dir = source_dir / "include";
    fs::path src_dir = source_dir / "src";

    if (!fs::exists(include_dir) || !fs::exists(src_dir)) {
        std::cerr << "Error: Directory layout must contain 'include' and 'src' under " << source_dir.string() << "\n";
        return 1;
    }

    const std::vector<std::string> headers = {
        "ksh_common.h",
        "ksh_types.h",
        "ksh_state.h",
        "ksh_terminal.h",
        "ksh_traps.h",
        "ksh_variables.h",
        "ksh_io.h",
        "ksh_jobs.h",
        "ksh_parser.h",
        "ksh_expansion.h",
        "ksh_builtins.h",
        "ksh_executor.h",
        "ksh_startup.h",
        "ksh_selftest.h"
    };

    const std::vector<std::string> sources = {
        "ksh_state.cpp",
        "ksh_traps.cpp",
        "ksh_variables.cpp",
        "ksh_io.cpp",
        "ksh_startup.cpp",
        "ksh_parser.cpp",
        "ksh_expansion.cpp",
        "ksh_jobs.cpp",
        "ksh_terminal.cpp",
        "ksh_builtins.cpp",
        "ksh_executor.cpp",
        "ksh_selftest.cpp",
        "ksh_main.cpp"
    };

    std::ofstream out(output_file, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Error: Failed to open output file " << output_file.string() << "\n";
        return 1;
    }

    out << "/*\n"
        << " * BSD 3-Clause License\n"
        << " *\n"
        << " * Copyright (c) 2026, Roberto J Dohnert\n"
        << " * All rights reserved.\n"
        << " *\n"
        << " * Redistribution and use in source and binary forms, with or without\n"
        << " * modification, are permitted provided that the following conditions are met:\n"
        << " *\n"
        << " * 1. Redistributions of source code must retain the above copyright notice, this\n"
        << " *    list of conditions and the following disclaimer.\n"
        << " *\n"
        << " * 2. Redistributions in binary form must reproduce the above copyright notice,\n"
        << " *    this list of conditions and the following disclaimer in the documentation\n"
        << " *    and/or other materials provided with the distribution.\n"
        << " *\n"
        << " * 3. Neither the name of the copyright holder nor the names of its\n"
        << " *    contributors may be used to endorse or promote products derived from\n"
        << " *    this software without specific prior written permission.\n"
        << " *\n"
        << " * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\"\n"
        << " * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n"
        << " * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n"
        << " * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE\n"
        << " * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n"
        << " * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR\n"
        << " * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER\n"
        << " * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,\n"
        << " * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n"
        << " * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"
        << " */\n\n"
        << "/******************************************************************************\n"
        << "** This file is an amalgamation of all CrossShell KSH modular source files.\n"
        << "** It was generated automatically by amalgamate.exe / amalgamate.ps1.\n"
        << "**\n"
        << "** SQLite-Style Amalgamation Design:\n"
        << "** - Primary development occurs modularly in src/ksh/include/ and src/ksh/src/\n"
        << "** - Single-file drop-in compilation is provided by this amalgamated file\n"
        << "** - Zero third-party runtime dependencies; standard C++17 and Win32 only\n"
        << "******************************************************************************/\n\n"
        << "#ifndef KSH_AMALGAMATION\n"
        << "#define KSH_AMALGAMATION 1\n"
        << "#endif\n\n"
        << "/*\n"
        << "*******************************************************************************\n"
        << "** SECTION I: HEADER DECLARATIONS AND TYPE DEFINITIONS\n"
        << "*******************************************************************************\n"
        << "*/\n\n";

    for (const auto& header : headers) {
        fs::path p = include_dir / header;
        std::cout << "Merging header: " << header << "\n";
        out << "/******************************************************************************\n"
            << "** Begin file " << header << "\n"
            << "******************************************************************************/\n";
        process_file(p, true, out);
        out << "/******************************************************************************\n"
            << "** End of " << header << "\n"
            << "******************************************************************************/\n\n";
    }

    out << "/*\n"
        << "*******************************************************************************\n"
        << "** SECTION II: IMPLEMENTATION UNITS\n"
        << "*******************************************************************************\n"
        << "*/\n\n";

    for (const auto& src : sources) {
        fs::path p = src_dir / src;
        std::cout << "Merging source: " << src << "\n";
        out << "/******************************************************************************\n"
            << "** Begin file " << src << "\n"
            << "******************************************************************************/\n";
        process_file(p, false, out);
        out << "/******************************************************************************\n"
            << "** End of " << src << "\n"
            << "******************************************************************************/\n\n";
    }

    std::cout << "Amalgamation successfully created: " << output_file.string() << "\n";
    return 0;
}
