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

#include "port_engine.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "help.hpp"
#include <iostream>
#include <sstream>
#include <set>
#include <filesystem>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;
using std::string;
using std::vector;
using std::set;
using std::cout;
using std::cerr;

namespace PortModule {

vector<string> parse_pe_import_dlls(const string& exe_path) {
    vector<string> required_dlls;
#if defined(PLATFORM_WINDOWS)
    HANDLE file = CreateFileA(exe_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return required_dlls;
    }

    HANDLE mapping = CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping) {
        CloseHandle(file);
        return required_dlls;
    }

    void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!view) {
        CloseHandle(mapping);
        CloseHandle(file);
        return required_dlls;
    }

    const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(view);
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
        UnmapViewOfFile(view);
        CloseHandle(mapping);
        CloseHandle(file);
        return required_dlls;
    }

    const auto* nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const BYTE*>(view) + dos_header->e_lfanew);
    if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
        UnmapViewOfFile(view);
        CloseHandle(mapping);
        CloseHandle(file);
        return required_dlls;
    }

    const auto* import_dir = &nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (import_dir->Size == 0 || import_dir->VirtualAddress == 0) {
        UnmapViewOfFile(view);
        CloseHandle(mapping);
        CloseHandle(file);
        return required_dlls;
    }

    const auto* import_desc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(reinterpret_cast<const BYTE*>(view) + import_dir->VirtualAddress);
    for (; import_desc->Name != 0; ++import_desc) {
        const char* dll_name = reinterpret_cast<const char*>(reinterpret_cast<const BYTE*>(view) + import_desc->Name);
        if (dll_name) {
            required_dlls.push_back(dll_name);
        }
    }

    UnmapViewOfFile(view);
    CloseHandle(mapping);
    CloseHandle(file);
#else
    (void)exe_path;
#endif
    return required_dlls;
}

void resolve_pe_dll_dependencies(const string& msys_root, const string& subsystem, const string& exe_path) {
    cout << "port: Dynamically inspecting PE imports for " << exe_path << "...\n";
    string bin_subdir = "mingw64";
    if (subsystem == "UCRT64") bin_subdir = "ucrt64";
    else if (subsystem == "CLANG64") bin_subdir = "clang64";
    string bin_dir = (fs::path(msys_root) / bin_subdir / "bin").string();
    fs::path target_dir = fs::path(exe_path).parent_path();

    set<string> required_dlls;
    for (const auto& dll : parse_pe_import_dlls(exe_path)) {
        required_dlls.insert(dll);
    }

    for (const auto& dll : required_dlls) {
        fs::path src_dll = fs::path(bin_dir) / dll;
        fs::path dst_dll = target_dir / dll;
        if (fs::exists(src_dll) && !fs::exists(dst_dll)) {
            cout << "  Bundled Dependency: " << dll << " -> " << target_dir.string() << "\n";
            fs::copy_file(src_dll, dst_dll, fs::copy_options::overwrite_existing);
        }
    }
}

void run_gnu_port(const vector<string>& args) {
    string msys_root = find_msys2_root();
    if (msys_root.empty()) msys_root = "C:\\msys64";
    string subsystem = "MINGW64";
    string source_dir = ".";
    bool run_autoreconf = true;
    bool bundle_dlls = true;

    for (size_t i = 0; i < args.size(); ++i) {
        string arg = args[i];
        if (arg == "-h" || arg == "--help") { print_port_help(); return; }
        else if (arg == "--no-autoreconf") run_autoreconf = false;
        else if (arg == "--no-dll-bundle") bundle_dlls = false;
        else if (arg.rfind("--msys2=", 0) == 0) msys_root = arg.substr(8);
        else if (arg.rfind("--subsystem=", 0) == 0) subsystem = arg.substr(12);
        else if (arg[0] != '-') source_dir = arg;
    }

    string bash_exe = (fs::path(msys_root) / "usr" / "bin" / "bash.exe").string();
    if (!fs::exists(bash_exe)) {
        cerr << "port: error: MSYS2 bash not found at " << bash_exe << "\n";
        return;
    }

    string posix_src = win_to_posix_path(source_dir);
    cout << "port: Automated build pipeline starting for " << posix_src << " (" << subsystem << ")...\n";

    std::stringstream pipeline;
    string bash_command = "export MSYSTEM=" + subsystem + " && source /etc/profile && cd " + shell_quote(posix_src) + " && " +
                          (run_autoreconf ? "(test -f configure || autoreconf -fiv) && " : "") +
                          "./configure && make -j4";
    pipeline << shell_quote(bash_exe) << " --login -c " << shell_quote(bash_command);

    string out;
    if (run_cmd(pipeline.str(), out) == 0) {
        cout << "port: Build succeeded.\n";
        if (bundle_dlls) {
            for (const auto& entry : fs::recursive_directory_iterator(source_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".exe") {
                    resolve_pe_dll_dependencies(msys_root, subsystem, entry.path().string());
                }
            }
        }
    } else {
        cerr << "port: Build failed:\n" << out << "\n";
    }
}

} // namespace PortModule
