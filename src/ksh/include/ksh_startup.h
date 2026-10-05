/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
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

#ifndef CROSSSHELL_KSH_STARTUP_H
#define CROSSSHELL_KSH_STARTUP_H

#include "ksh_types.h"

bool parse_startup_arguments(int argc, wchar_t* argv[], StartupOptions& options, int& first_positional_index, bool& show_help, bool& show_version);
bool file_exists_regular(const std::wstring& path);
bool ensure_default_home_kshrc_exists(const std::wstring& profile_path);
std::wstring get_default_startup_profile_path();
bool load_startup_profile(const StartupOptions& options, bool& should_exit_shell);

std::wstring resolve_history_file_path();
bool resolve_history_recall(const std::wstring& input, std::wstring& resolved);
void load_command_history_from_file();
bool append_history_entry(const std::wstring& command_line);
void append_history_entry_to_file(const std::wstring& command_line);
void append_command_to_history_file(const std::wstring& command);
void rewrite_history_file();
void enforce_history_size_limit(bool rewrite_file);
bool history_timestamps_enabled();
bool history_dedupe_enabled();
size_t history_size_limit();
std::wstring format_history_timestamp();

std::wstring get_windows_release_text();
void print_version_text(const std::function<bool(const std::wstring&)>& write_output);
void print_startup_system_info();
std::wstring format_bytes_iec(ULONGLONG bytes);

#endif // CROSSSHELL_KSH_STARTUP_H
