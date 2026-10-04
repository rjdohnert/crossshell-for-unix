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

#ifndef AUTOTOOL_AUTOCONF_HPP
#define AUTOTOOL_AUTOCONF_HPP

#include <string>
#include <vector>
#include <map>
#include <utility>

namespace AutoconfModule {

struct AutoconfProject {
    std::string package_name = "package";
    std::string package_version = "1.0";
    bool check_cc = false;
    bool check_cxx = false;
    std::vector<std::string> config_headers;
    std::vector<std::string> check_headers;
    std::vector<std::string> check_funcs;
    std::vector<std::string> check_types;
    std::vector<std::string> check_sizeof;
    std::vector<std::string> check_decls;
    std::vector<std::string> check_members;
    std::vector<std::pair<std::string, std::vector<std::string>>> search_libs;
    std::vector<std::string> aux_dirs;
    std::vector<std::string> src_dirs;
    std::vector<std::string> subst_files;
    std::vector<std::string> config_files;
    std::vector<std::string> output_files;
    std::vector<std::string> subst_names;
    std::map<std::string, std::string> subst_vars;
    std::map<std::string, std::string> subst_values;
    std::map<std::string, std::string> defines;
};

void run_autoconf(const std::vector<std::string>& args);

} // namespace AutoconfModule

#endif // AUTOTOOL_AUTOCONF_HPP
