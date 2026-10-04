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

#ifndef AUTOTOOL_LIBTOOL_HPP
#define AUTOTOOL_LIBTOOL_HPP

#include <string>
#include <vector>

namespace LibtoolModule {

enum class Mode { NONE, COMPILE, LINK, INSTALL, CLEAN, FINISH };

struct Options {
    Mode mode = Mode::NONE;
    std::string tag = "CXX";
    std::string compiler;
    std::string output;
    std::string cflags;
    std::string ldflags;
    std::string libs;
    std::string install_dir;
    std::string runtime_path;
    std::string release_name;
    std::string version_info;
    bool verbose = false;
    bool dry_run = false;
    bool build_shared = false;
    bool build_static = false;
    bool build_module = false;
    bool avoid_version = false;
    bool no_install = false;
    std::vector<std::string> inputs;
};

int run_libtool(const std::vector<std::string>& args);

} // namespace LibtoolModule

#endif // AUTOTOOL_LIBTOOL_HPP
