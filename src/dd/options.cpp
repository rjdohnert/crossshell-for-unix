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

#include "options.hpp"
#include "size_parser.hpp"
#include <iostream>
#include <sstream>

void OptionParser::DisplayHelp(const char* prog_name) {
    (void)prog_name;
    std::cout << R"(dd(1)                   CrossShell for UNIX Reference Manual                  dd(1)

    NAME
        dd - convert and copy a file

    SYNOPSIS
        dd [OPERANDS...]
        dd [OPTIONS]

    DESCRIPTION
        Copy a file, converting and formatting according to the operands.

    OPERANDS
        if=FILE
            Read from FILE instead of standard input.

        of=FILE
            Write to FILE instead of standard output.

        bs=BYTES
            Read and write up to BYTES bytes at a time (default: 512).

        ibs=BYTES, obs=BYTES
            Read or write up to BYTES bytes at a time.

        count=N
            Copy only N input blocks.

        skip=N
            Skip N ibs-sized blocks at start of input.

        seek=N
            Skip N obs-sized blocks at start of output.

        status=LEVEL
            Transfer diagnostic level: 'none', 'noxfer', or 'progress'.

        conv=CONVS
            Comma-separated conversion list: ucase, lcase, swab, sync,
            noerror, notrunc, excl.

    OPTIONS
        --json, --csv, --table
            Emit transfer status records in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send status output directly through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        dd if=input.img of=output.img bs=4k count=100
            Copy 100 4KB blocks from input.img to output.img.

        dd if=source.dat of=dest.dat conv=ucase
            Convert source text to uppercase while copying.

        dd if=raw.bin of=backup.bin bs=1M status=progress
            Copy with progress indicator.

    CrossShell for UNIX                                                    dd(1)
)";
}

void OptionParser::DisplayVersion() {
    std::cout << "dd (Extended Utilities) 2.0\n"
              << "Copyright (C) 2026 Roberto J. Dohnert\n";
}

bool OptionParser::Parse(int argc, char* argv[], DdOptions& opt, bool& exitEarly) const {
    exitEarly = false;
    bool end_of_opts = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (!end_of_opts) {
            if (arg == "--help" || arg == "-h") {
                DisplayHelp(argv[0]);
                exitEarly = true;
                return true;
            }
            if (arg == "--version" || arg == "-V") {
                DisplayVersion();
                exitEarly = true;
                return true;
            }
            if (arg == "--json") { opt.output_format = 1; continue; }
            if (arg == "--csv") { opt.output_format = 2; continue; }
            if (arg == "--table") { opt.output_format = 3; continue; }
            if (arg == "--pipe" && i + 1 < argc) { opt.pipe_command = argv[++i]; continue; }
            if (arg == "--") { end_of_opts = true; continue; }
        }

        size_t eq_pos = arg.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = arg.substr(0, eq_pos);
            std::string val = arg.substr(eq_pos + 1);

            if (key == "if") opt.if_path = val;
            else if (key == "of") opt.of_path = val;
            else if (key == "bs") {
                uint64_t sz = SizeParser::ParseSize(val);
                if (sz > 0) { opt.ibs = sz; opt.obs = sz; }
            }
            else if (key == "ibs") {
                uint64_t sz = SizeParser::ParseSize(val);
                if (sz > 0) opt.ibs = sz;
            }
            else if (key == "obs") {
                uint64_t sz = SizeParser::ParseSize(val);
                if (sz > 0) opt.obs = sz;
            }
            else if (key == "count") opt.count = SizeParser::ParseSize(val);
            else if (key == "skip") opt.skip = SizeParser::ParseSize(val);
            else if (key == "seek") opt.seek = SizeParser::ParseSize(val);
            else if (key == "status") {
                if (val == "none") opt.status = StatusLevel::NONE;
                else if (val == "noxfer") opt.status = StatusLevel::NOXFER;
                else if (val == "progress") opt.status = StatusLevel::PROGRESS;
            }
            else if (key == "conv") {
                std::stringstream ss(val);
                std::string conv_item;
                while (std::getline(ss, conv_item, ',')) {
                    if (conv_item == "ucase") opt.conv_ucase = true;
                    else if (conv_item == "lcase") opt.conv_lcase = true;
                    else if (conv_item == "swab") opt.conv_swab = true;
                    else if (conv_item == "sync") opt.conv_sync = true;
                    else if (conv_item == "noerror") opt.conv_noerror = true;
                    else if (conv_item == "notrunc") opt.conv_notrunc = true;
                    else if (conv_item == "excl") opt.conv_excl = true;
                }
            }
        }
    }
    return true;
}
