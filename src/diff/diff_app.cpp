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

#include "diff_app.hpp"
#include "models.hpp"
#include "arg_parser.hpp"
#include "line_reader.hpp"
#include "myers_diff.hpp"
#include "formatter.hpp"
#include <iostream>
#include <memory>

namespace DiffUtil {

int DiffApp::run(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Options opts;
    if (!ArgParser::parse(argc, argv, opts)) {
        return static_cast<int>(ExitCode::Error);
    }

    if (opts.show_help) {
        HelpSystem::print_help();
        return static_cast<int>(ExitCode::Identical);
    }

    if (opts.show_version) {
        HelpSystem::print_version();
        return static_cast<int>(ExitCode::Identical);
    }

    std::vector<std::string> raw1, raw2;
    std::vector<std::string> norm1, norm2;

    if (!LineReader::read_all_lines(opts.file1, raw1, norm1, opts)) return static_cast<int>(ExitCode::Error);
    if (!LineReader::read_all_lines(opts.file2, raw2, norm2, opts)) return static_cast<int>(ExitCode::Error);

    auto edits = MyersDiff::compute(norm1, norm2);

    bool differs = false;
    for (const auto& item : edits) {
        if (item.type != EditType::Keep) {
            differs = true;
            break;
        }
    }

    if (!differs) {
        if (opts.report_identical) {
            std::cout << "Files " << opts.file1 << " and " << opts.file2 << " are identical\n";
        }
        return static_cast<int>(ExitCode::Identical);
    }

    std::unique_ptr<IOutputFormatter> formatter;
    if (opts.format == DiffFormat::Brief) {
        formatter = std::make_unique<BriefFormatter>();
    } else if (opts.format == DiffFormat::Unified) {
        formatter = std::make_unique<UnifiedFormatter>();
    } else {
        formatter = std::make_unique<NormalFormatter>();
    }

    formatter->format(edits, raw1, raw2, opts);
    return static_cast<int>(ExitCode::Differ);
}

} // namespace DiffUtil
