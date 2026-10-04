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

#ifndef AUTOTOOL_UTILS_HPP
#define AUTOTOOL_UTILS_HPP

#include "types.hpp"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <filesystem>

extern std::mutex global_log_mutex;

std::string trim(const std::string& str);
bool starts_with(const std::string& value, const std::string& prefix);
std::string to_upper_identifier(std::string str);
std::string unquote_m4(const std::string& str);
std::vector<std::string> split_words(const std::string& text);
std::vector<std::string> split_m4_args(const std::string& text);
std::string shell_quote(const std::string& value);
std::string win_to_posix_path(const std::string& win_path);

int run_cmd(const std::string& cmd, std::string& output);
std::string get_env_var(const char* name);
std::string trim_path(const std::string& path);
bool path_exists(const std::string& path);

std::string find_executable_on_path(const std::vector<std::string>& names);
std::string find_vcvars_bat_path();
std::string find_msys2_root();
std::string find_compiler_path(const std::string& compiler_name);
CompilerType detect_compiler();

std::string replace_all(const std::string& input, const std::string& from, const std::string& to);
bool parse_m4_macro_call(const std::string& line, std::string& name, std::vector<std::string>& args);
std::string expand_m4_template(const std::string& text, const std::map<std::string, std::string>& substitutions);

#if defined(PLATFORM_WINDOWS)
bool initialize_msvc_environment(const std::string& vcvars_bat, std::string& error);
#endif

#endif // AUTOTOOL_UTILS_HPP
