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

#ifndef CROSSSHELL_KSH_IO_H
#define CROSSSHELL_KSH_IO_H

#include "ksh_types.h"

bool is_custom_fd_in_range(int fd);
bool is_valid_handle_value(HANDLE handle);
std::array<HANDLE, kCustomFdTableSize> make_invalid_custom_fd_table();
HANDLE get_persistent_custom_fd(int fd);
bool set_persistent_custom_fd(int fd, HANDLE handle, std::wstring& error_message);
bool close_persistent_custom_fd(int fd, std::wstring& error_message);
bool duplicate_persistent_custom_fd(int source_fd, int target_fd, std::wstring& error_message);
bool apply_persistent_custom_fd_actions(const RedirectionSpec& redir, std::wstring& error_message);

bool try_parse_custom_fd_redirection_token(const std::wstring& token, const std::vector<std::wstring>& tokens, size_t& index, RedirectionSpec& redir, std::wstring& error_message, bool& handled);
bool parse_redirections(std::vector<std::wstring>& tokens, RedirectionSpec& redir, std::wstring& error_message);
bool has_any_redirection(const RedirectionSpec& redir);
bool setup_redirection_handles(const RedirectionSpec* redir, HANDLE& std_in, HANDLE& std_out, HANDLE& std_err, bool& close_in, bool& close_out, bool& close_err);
void close_redirection_handles(HANDLE std_in, HANDLE std_out, HANDLE std_err, bool close_in, bool close_out, bool close_err);

std::wstring translate_device_path(const std::wstring& path);
bool open_redirection_file(const std::wstring& path, DWORD access, DWORD creation, HANDLE& out_handle);
bool write_text_with_stdout_redirection(const std::wstring& text, const RedirectionSpec& redir);

#endif // CROSSSHELL_KSH_IO_H
