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

    // Get the file size so all offsets can be bounds-checked.
    LARGE_INTEGER file_size{};
    if (!GetFileSizeEx(file, &file_size)) {
        CloseHandle(mapping);
        CloseHandle(file);
        return required_dlls;
    }
    const size_t view_size = static_cast<size_t>(file_size.QuadPart);

    void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!view) {
        CloseHandle(mapping);
        CloseHandle(file);
        return required_dlls;
    }

    // Bounds-check helper: true if [offset, offset+size) is within the mapping.
    auto in_bounds = [&](size_t offset, size_t size) -> bool {
        return offset <= view_size && size <= view_size - offset;
    };

    auto cleanup = [&]() {
        UnmapViewOfFile(view);
        CloseHandle(mapping);
        CloseHandle(file);
    };

    if (!in_bounds(0, sizeof(IMAGE_DOS_HEADER))) { cleanup(); return required_dlls; }
    const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(view);
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
        cleanup();
        return required_dlls;
    }

    // Validate e_lfanew before dereferencing the NT headers.
    size_t nt_offset = static_cast<size_t>(dos_header->e_lfanew);
    if (!in_bounds(nt_offset, sizeof(IMAGE_NT_HEADERS))) { cleanup(); return required_dlls; }
    const auto* nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const BYTE*>(view) + nt_offset);
    if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
        cleanup();
        return required_dlls;
    }

    const auto* import_dir = &nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (import_dir->Size == 0 || import_dir->VirtualAddress == 0) {
        cleanup();
        return required_dlls;
    }

    // Validate the import descriptor table start.
    size_t desc_offset = static_cast<size_t>(import_dir->VirtualAddress);
    if (!in_bounds(desc_offset, sizeof(IMAGE_IMPORT_DESCRIPTOR))) { cleanup(); return required_dlls; }

    const auto* import_desc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(reinterpret_cast<const BYTE*>(view) + desc_offset);
    size_t desc_index = 0;
    const size_t max_descriptors = (view_size - desc_offset) / sizeof(IMAGE_IMPORT_DESCRIPTOR);
    while (import_desc->Name != 0 && desc_index < max_descriptors) {
        size_t name_offset = static_cast<size_t>(import_desc->Name);
        // Validate the name pointer and that a NUL terminator exists in bounds.
        if (in_bounds(name_offset, 1)) {
            const char* dll_name = reinterpret_cast<const char*>(reinterpret_cast<const BYTE*>(view) + name_offset);
            size_t max_len = view_size - name_offset;
            size_t len = strnlen(dll_name, max_len);
            if (len > 0 && len < max_len) {
                required_dlls.push_back(string(dll_name, len));
            }
        }
        ++import_desc;
        ++desc_index;
        if (!in_bounds(desc_offset + (desc_index + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR), sizeof(IMAGE_IMPORT_DESCRIPTOR))) break;
    }

    cleanup();
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
        // Validate the DLL name: reject traversal/absolute paths so a malformed
        // PE import cannot escape the target directory.
        if (dll.empty() || dll.find("..") != string::npos ||
            dll.find('/') != string::npos || dll.find('\\') != string::npos ||
            dll.find(':') != string::npos) {
            continue;
        }
        fs::path src_dll = fs::path(bin_dir) / dll;
        fs::path dst_dll = target_dir / dll;
        std::error_code ec;
        if (fs::exists(src_dll, ec) && !fs::exists(dst_dll, ec)) {
            cout << "  Bundled Dependency: " << dll << " -> " << target_dir.string() << "\n";
            fs::copy_file(src_dll, dst_dll, fs::copy_options::overwrite_existing, ec);
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
