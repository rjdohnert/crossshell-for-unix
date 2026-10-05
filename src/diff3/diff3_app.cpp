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

#include "diff3_app.hpp"
#include "models.hpp"
#include "arg_parser.hpp"
#include "line_reader.hpp"
#include "diff3_engine.hpp"
#include "formatter.hpp"
#include <iostream>
#include <memory>

namespace Diff3Util {

int Diff3App::run(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Options opts;
    if (!ArgParser::parse(argc, argv, opts)) {
        return static_cast<int>(ExitCode::Error);
    }

    if (opts.show_help) {
        HelpSystem::print_help();
        return static_cast<int>(ExitCode::SuccessNoConflicts);
    }

    if (opts.show_version) {
        HelpSystem::print_version();
        return static_cast<int>(ExitCode::SuccessNoConflicts);
    }

    std::vector<std::string> lines_mine, lines_base, lines_yours;

    if (!LineReader::read_file(opts.file_mine, lines_mine, opts.strip_trailing_cr)) return static_cast<int>(ExitCode::Error);
    if (!LineReader::read_file(opts.file_old, lines_base, opts.strip_trailing_cr)) return static_cast<int>(ExitCode::Error);
    if (!LineReader::read_file(opts.file_yours, lines_yours, opts.strip_trailing_cr)) return static_cast<int>(ExitCode::Error);

    auto chunks = Diff3Engine::align(lines_mine, lines_base, lines_yours);

    std::unique_ptr<IOutputFormatter> formatter;
    if (opts.mode == OutputMode::Merge) {
        formatter = std::make_unique<MergeFormatter>();
    } else if (opts.mode == OutputMode::Traditional) {
        formatter = std::make_unique<TraditionalFormatter>();
    } else {
        formatter = std::make_unique<EdFormatter>();
    }

    bool has_conflicts = formatter->format(chunks, opts);

    return has_conflicts ? static_cast<int>(ExitCode::ConflictsFound)
                         : static_cast<int>(ExitCode::SuccessNoConflicts);
}

} // namespace Diff3Util
