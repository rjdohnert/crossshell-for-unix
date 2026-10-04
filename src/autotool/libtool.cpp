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

#include "libtool.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "help.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;
using std::string;
using std::vector;
using std::cout;
using std::cerr;

namespace LibtoolModule {

string normalize_tag(const string& tag) {
    string upper = tag;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(toupper(c));
    });
    return upper.empty() ? "CXX" : upper;
}

bool is_source_file(const string& path) {
    string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(tolower(c));
    });
    return ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx";
}

bool is_object_file(const string& path) {
    string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(tolower(c));
    });
    return ext == ".o" || ext == ".obj" || ext == ".lo";
}

string mode_to_string(Mode mode) {
    switch (mode) {
        case Mode::COMPILE: return "compile";
        case Mode::LINK: return "link";
        case Mode::INSTALL: return "install";
        case Mode::CLEAN: return "clean";
        case Mode::FINISH: return "finish";
        default: return "none";
    }
}

CompilerType compiler_type_from_name(const string& compiler_name) {
    if (compiler_name.empty()) {
        return detect_compiler();
    }

    string lower = compiler_name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(tolower(c));
    });
    if (lower.find("clang-cl") != string::npos || lower.find("clang++") != string::npos || lower.find("clang") != string::npos) {
        return CompilerType::CLANG;
    }
    if (lower.find("g++") != string::npos || lower.find("gcc") != string::npos) {
        return CompilerType::GCC;
    }
    if (lower.find("xlc") != string::npos) {
        return CompilerType::XLC;
    }
    if (lower.find("cl") != string::npos) {
        return CompilerType::MSVC;
    }
    return detect_compiler();
}

string default_compiler_for_tag(CompilerType compiler, const string& tag) {
    const bool use_cxx = normalize_tag(tag) != "CC";
    switch (compiler) {
        case CompilerType::MSVC: return "cl.exe";
        case CompilerType::CLANG: return use_cxx ? "clang++" : "clang";
        case CompilerType::GCC: return use_cxx ? "g++" : "gcc";
        case CompilerType::XLC: return use_cxx ? "xlc++" : "xlc";
        default: return use_cxx ? "c++" : "cc";
    }
}

bool ensure_toolchain_ready(CompilerType compiler, string& error) {
#if defined(PLATFORM_WINDOWS)
    if (compiler != CompilerType::MSVC) {
        return true;
    }

    if (!find_compiler_path("cl.exe").empty()) {
        return true;
    }

    string vcvars_bat = find_vcvars_bat_path();
    if (vcvars_bat.empty()) {
        error = "unable to locate vcvars64.bat / VsDevCmd.bat";
        return false;
    }

    return initialize_msvc_environment(vcvars_bat, error);
#else
    (void)compiler;
    error.clear();
    return true;
#endif
}

bool run_native_command(const string& cmd, const Options& opts, const string& prefix) {
    if (opts.verbose || opts.dry_run) {
        cout << prefix << cmd << "\n";
    }
    if (opts.dry_run) {
        return true;
    }

    string output;
    int status = run_cmd(cmd, output);
    if (status != 0) {
        cerr << "libtool: command failed (mode=" << mode_to_string(opts.mode) << ")\n";
        if (!output.empty()) {
            cerr << output << "\n";
        }
        return false;
    }

    if (opts.verbose && !output.empty()) {
        cout << output;
        if (output.back() != '\n') {
            cout << "\n";
        }
    }
    return true;
}

string native_object_output(const string& requested_output, const string& source_path) {
    fs::path output_path = requested_output.empty() ? fs::path(source_path).replace_extension(OBJ_EXT) : fs::path(requested_output);
    if (output_path.extension() == ".lo") {
        output_path.replace_extension(OBJ_EXT);
    }
    return output_path.string();
}

bool compile_source(const Options& opts, CompilerType compiler_type, const string& compiler, const string& source_path, const string& object_path) {
    fs::path output_path = fs::path(object_path);
    if (output_path.has_parent_path() && !output_path.parent_path().empty()) {
        fs::create_directories(output_path.parent_path());
    }

    const bool use_cxx = normalize_tag(opts.tag) != "CC";
    string cmd = shell_quote(compiler);
    if (compiler_type == CompilerType::MSVC) {
        cmd += " /nologo";
        if (use_cxx) {
            cmd += " /EHsc";
        }
        if (!opts.cflags.empty()) {
            cmd += " " + opts.cflags;
        }
        cmd += " /c " + shell_quote(source_path) + " /Fo" + shell_quote(object_path);
    } else {
        if (!opts.cflags.empty()) {
            cmd += " " + opts.cflags;
        }
        cmd += " -c " + shell_quote(source_path) + " -o " + shell_quote(object_path);
    }

    return run_native_command(cmd, opts, "libtool: compile: ");
}

bool collect_link_inputs(const Options& opts, CompilerType compiler_type, const string& compiler, vector<string>& objects) {
    fs::path output_parent = opts.output.empty() ? fs::current_path() : fs::path(opts.output).parent_path();
    if (output_parent.empty()) {
        output_parent = fs::current_path();
    }

    for (const auto& input : opts.inputs) {
        if (is_source_file(input)) {
            fs::path generated = output_parent / fs::path(input).filename();
            generated.replace_extension(OBJ_EXT);
            if (!compile_source(opts, compiler_type, compiler, input, generated.string())) {
                return false;
            }
            objects.push_back(generated.string());
        } else {
            objects.push_back(input);
        }
    }
    return true;
}

bool run_compile_mode(const Options& opts) {
    if (opts.inputs.empty()) {
        cerr << "libtool: compile mode requires at least one source file\n";
        return false;
    }
    if (opts.inputs.size() > 1 && !opts.output.empty()) {
        cerr << "libtool: compile mode accepts '-o' only with a single source\n";
        return false;
    }

    CompilerType compiler_type = compiler_type_from_name(opts.compiler);
    string error;
    if (!ensure_toolchain_ready(compiler_type, error)) {
        cerr << "libtool: " << error << "\n";
        return false;
    }

    string compiler = opts.compiler.empty() ? default_compiler_for_tag(compiler_type, opts.tag) : opts.compiler;
    for (size_t i = 0; i < opts.inputs.size(); ++i) {
        const string& source_path = opts.inputs[i];
        if (!is_source_file(source_path)) {
            cerr << "libtool: compile mode expects source files, got '" << source_path << "'\n";
            return false;
        }

        string object_path = native_object_output(i == 0 ? opts.output : "", source_path);
        if (!compile_source(opts, compiler_type, compiler, source_path, object_path)) {
            return false;
        }
    }
    return true;
}

bool run_link_mode(const Options& opts) {
    if (opts.inputs.empty()) {
        cerr << "libtool: link mode requires at least one source, object, or library input\n";
        return false;
    }

    CompilerType compiler_type = compiler_type_from_name(opts.compiler);
    string error;
    if (!ensure_toolchain_ready(compiler_type, error)) {
        cerr << "libtool: " << error << "\n";
        return false;
    }

    string compiler = opts.compiler.empty() ? default_compiler_for_tag(compiler_type, opts.tag) : opts.compiler;
    vector<string> link_inputs;
    if (!collect_link_inputs(opts, compiler_type, compiler, link_inputs)) {
        return false;
    }

    string output = opts.output;
    if (output.empty()) {
        output = opts.build_static ? ("libtool" + LIB_EXT) : (opts.build_shared || opts.build_module ? ("libtool" + DLL_EXT) : ("a" + EXE_EXT));
    }

    fs::path output_path = fs::path(output);
    if (output_path.has_parent_path() && !output_path.parent_path().empty()) {
        fs::create_directories(output_path.parent_path());
    }

    string joined_inputs;
    for (const auto& input : link_inputs) {
        joined_inputs += " " + shell_quote(input);
    }

    string cmd;
    if (opts.build_static) {
#if defined(PLATFORM_WINDOWS)
        cmd = "lib.exe /nologo /OUT:" + shell_quote(output) + joined_inputs;
#else
        cmd = "ar rcs " + shell_quote(output) + joined_inputs;
#endif
    } else if (opts.build_shared || opts.build_module) {
        cmd = shell_quote(compiler);
        if (compiler_type == CompilerType::MSVC) {
            cmd += " /nologo /LD /Fe" + shell_quote(output) + joined_inputs;
        } else {
            cmd += " -shared -o " + shell_quote(output) + joined_inputs;
        }
        if (!opts.ldflags.empty()) {
            cmd += " " + opts.ldflags;
        }
        if (!opts.libs.empty()) {
            cmd += " " + opts.libs;
        }
    } else {
        cmd = shell_quote(compiler);
        if (compiler_type == CompilerType::MSVC) {
            cmd += " /nologo /Fe" + shell_quote(output) + joined_inputs;
        } else {
            cmd += " -o " + shell_quote(output) + joined_inputs;
        }
        if (!opts.ldflags.empty()) {
            cmd += " " + opts.ldflags;
        }
        if (!opts.libs.empty()) {
            cmd += " " + opts.libs;
        }
    }

    return run_native_command(cmd, opts, "libtool: link: ");
}

bool run_install_mode(const Options& opts) {
    if (opts.inputs.empty()) {
        cerr << "libtool: install mode requires at least one source artifact\n";
        return false;
    }

    fs::path destination = opts.install_dir.empty() ? fs::path(opts.inputs.back()) : fs::path(opts.install_dir);
    const bool explicit_destination = !opts.install_dir.empty();
    size_t source_count = explicit_destination ? opts.inputs.size() : opts.inputs.size() - 1;
    if (!explicit_destination && opts.inputs.size() < 2) {
        cerr << "libtool: install mode requires a source and destination\n";
        return false;
    }

    const bool dest_is_directory = explicit_destination || source_count > 1 || destination.extension().empty() || fs::is_directory(destination);
    if (dest_is_directory) {
        fs::create_directories(destination);
    } else if (destination.has_parent_path() && !destination.parent_path().empty()) {
        fs::create_directories(destination.parent_path());
    }

    for (size_t i = 0; i < source_count; ++i) {
        fs::path src = opts.inputs[i];
        if (!fs::exists(src)) {
            cerr << "libtool: install source missing: " << src.string() << "\n";
            return false;
        }

        fs::path target = dest_is_directory ? (destination / src.filename()) : destination;
        if (opts.verbose || opts.dry_run) {
            cout << "libtool: install: " << src.string() << " -> " << target.string() << "\n";
        }
        if (!opts.dry_run) {
            fs::copy_file(src, target, fs::copy_options::overwrite_existing);
        }
    }
    return true;
}

bool run_clean_mode(const Options& opts) {
    if (opts.inputs.empty()) {
        cerr << "libtool: clean mode requires one or more files\n";
        return false;
    }

    for (const auto& input : opts.inputs) {
        if (opts.verbose || opts.dry_run) {
            cout << "libtool: clean: " << input << "\n";
        }
        if (!opts.dry_run) {
            std::error_code ec;
            fs::remove(input, ec);
            if (ec && fs::exists(input)) {
                cerr << "libtool: failed to remove '" << input << "': " << ec.message() << "\n";
                return false;
            }
        }
    }
    return true;
}

bool run_finish_mode(const Options& opts) {
    string path = !opts.install_dir.empty() ? opts.install_dir : (!opts.runtime_path.empty() ? opts.runtime_path : (opts.inputs.empty() ? "" : opts.inputs.front()));
    if (path.empty()) {
        cerr << "libtool: finish mode requires a staging or runtime directory\n";
        return false;
    }

    if (opts.verbose || opts.dry_run) {
        cout << "libtool: finish: runtime directory '" << path << "'\n";
    }
    if (!opts.dry_run && !fs::exists(path)) {
        cerr << "libtool: finish target does not exist: " << path << "\n";
        return false;
    }
    return true;
}

int run_libtool(const vector<string>& args) {
    Options opts;

    for (size_t i = 0; i < args.size(); ++i) {
        const string& arg = args[i];
        if (arg == "-h" || arg == "--help") {
            print_libtool_help();
            return EXIT_SUCCESS;
        } else if (starts_with(arg, "--mode=")) {
            string mode = arg.substr(7);
            if (mode == "compile") opts.mode = Mode::COMPILE;
            else if (mode == "link") opts.mode = Mode::LINK;
            else if (mode == "install") opts.mode = Mode::INSTALL;
            else if (mode == "clean") opts.mode = Mode::CLEAN;
            else if (mode == "finish") opts.mode = Mode::FINISH;
            else {
                cerr << "libtool: unsupported mode '" << mode << "'\n";
                return EXIT_FAILURE;
            }
        } else if (starts_with(arg, "--tag=")) {
            opts.tag = normalize_tag(arg.substr(6));
        } else if (arg == "-o") {
            if (i + 1 >= args.size()) {
                cerr << "libtool: missing argument for -o\n";
                return EXIT_FAILURE;
            }
            opts.output = args[++i];
        } else if (starts_with(arg, "--output=")) {
            opts.output = arg.substr(9);
        } else if (arg == "-shared" || arg == "--shared") {
            opts.build_shared = true;
        } else if (arg == "-static" || arg == "--static") {
            opts.build_static = true;
        } else if (arg == "-module" || arg == "--module") {
            opts.build_module = true;
            opts.build_shared = true;
        } else if (arg == "-n" || arg == "--dry-run") {
            opts.dry_run = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "--silent") {
            opts.verbose = false;
        } else if (starts_with(arg, "--compiler=")) {
            opts.compiler = arg.substr(11);
        } else if (starts_with(arg, "--cflags=")) {
            opts.cflags = arg.substr(9);
        } else if (starts_with(arg, "--ldflags=")) {
            opts.ldflags = arg.substr(10);
        } else if (starts_with(arg, "--libs=")) {
            opts.libs = arg.substr(7);
        } else if (starts_with(arg, "--install-dir=")) {
            opts.install_dir = arg.substr(14);
        } else if (arg == "-rpath") {
            if (i + 1 >= args.size()) {
                cerr << "libtool: missing argument for -rpath\n";
                return EXIT_FAILURE;
            }
            opts.runtime_path = args[++i];
        } else if (starts_with(arg, "--rpath=")) {
            opts.runtime_path = arg.substr(8);
        } else if (arg == "-release") {
            if (i + 1 >= args.size()) {
                cerr << "libtool: missing argument for -release\n";
                return EXIT_FAILURE;
            }
            opts.release_name = args[++i];
        } else if (starts_with(arg, "-version-info=")) {
            opts.version_info = arg.substr(14);
        } else if (arg == "-version-info") {
            if (i + 1 >= args.size()) {
                cerr << "libtool: missing argument for -version-info\n";
                return EXIT_FAILURE;
            }
            opts.version_info = args[++i];
        } else if (arg == "-avoid-version") {
            opts.avoid_version = true;
        } else if (arg == "-no-install") {
            opts.no_install = true;
        } else if (arg == "-export-dynamic") {
            if (!opts.ldflags.empty()) {
                opts.ldflags += " ";
            }
            opts.ldflags += "-export-dynamic";
        } else if (!arg.empty() && arg[0] == '-' && opts.mode == Mode::COMPILE) {
            if (!opts.cflags.empty()) {
                opts.cflags += " ";
            }
            opts.cflags += arg;
        } else if (!arg.empty() && arg[0] == '-' && opts.mode == Mode::LINK) {
            if (starts_with(arg, "-l") || starts_with(arg, "-L") || starts_with(arg, "-Wl,")) {
                if (!opts.libs.empty()) {
                    opts.libs += " ";
                }
                opts.libs += arg;
            } else {
                if (!opts.ldflags.empty()) {
                    opts.ldflags += " ";
                }
                opts.ldflags += arg;
            }
        } else {
            opts.inputs.push_back(arg);
        }
    }

    if (opts.mode == Mode::NONE) {
        cerr << "libtool: missing required --mode switch\n";
        print_libtool_help();
        return EXIT_FAILURE;
    }

    if (opts.verbose) {
        if (!opts.release_name.empty()) {
            cout << "libtool: compatibility note: -release " << opts.release_name << " accepted as metadata\n";
        }
        if (!opts.version_info.empty()) {
            cout << "libtool: compatibility note: -version-info " << opts.version_info << " accepted as metadata\n";
        }
        if (opts.avoid_version) {
            cout << "libtool: compatibility note: -avoid-version accepted\n";
        }
        if (opts.no_install) {
            cout << "libtool: compatibility note: -no-install accepted\n";
        }
        if (!opts.runtime_path.empty()) {
            cout << "libtool: compatibility note: runtime/install path set to '" << opts.runtime_path << "'\n";
        }
    }

    bool ok = false;
    switch (opts.mode) {
        case Mode::COMPILE:
            ok = run_compile_mode(opts);
            break;
        case Mode::LINK:
            ok = run_link_mode(opts);
            break;
        case Mode::INSTALL:
            ok = run_install_mode(opts);
            break;
        case Mode::CLEAN:
            ok = run_clean_mode(opts);
            break;
        case Mode::FINISH:
            ok = run_finish_mode(opts);
            break;
        default:
            ok = false;
            break;
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace LibtoolModule
