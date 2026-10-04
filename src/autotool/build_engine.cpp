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

#include "build_engine.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "help.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <filesystem>

namespace fs = std::filesystem;
using std::string;
using std::vector;
using std::pair;
using std::thread;
using std::cout;
using std::cerr;
using std::lock_guard;
using std::mutex;

namespace BuildModule {

void run_native_build(const vector<string>& args) {
    int jobs = thread::hardware_concurrency();
    bool verbose = false;
    bool clean = false;

    for (size_t i = 0; i < args.size(); ++i) {
        string arg = args[i];
        if (arg == "-h" || arg == "--help") { print_build_help(); return; }
        else if (arg == "-v" || arg == "--verbose") verbose = true;
        else if (arg == "-c" || arg == "--clean") clean = true;
        else if (arg.rfind("-j", 0) == 0) {
            size_t eq = arg.find('=');
            if (eq != string::npos) jobs = std::stoi(arg.substr(eq + 1));
            else if (i + 1 < args.size()) jobs = std::stoi(args[++i]);
        }
    }

    CompilerType compiler = detect_compiler();
    if (compiler == CompilerType::UNKNOWN) {
        cerr << "build: error: no supported compiler detected.\n";
        return;
    }

    BuildTarget target;
    for (const auto& entry : fs::directory_iterator(".")) {
        if (entry.path().extension() == ".cpp" || entry.path().extension() == ".c") {
            target.sources.push_back(entry.path().string());
        }
    }

    if (target.sources.empty()) {
        cerr << "build: no source files (.cpp/.c) found in current directory.\n";
        return;
    }

    if (clean) {
        cout << "build: Cleaning build output directory '" << target.output_dir << "'...\n";
        fs::remove_all(target.output_dir);
    }

    fs::create_directories(target.output_dir);
    vector<string> objects;
    vector<pair<string, string>> tasks;

    for (const auto& src : target.sources) {
        string obj = (fs::path(target.output_dir) / (fs::path(src).stem().string() + OBJ_EXT)).string();
        objects.push_back(obj);
        if (!fs::exists(obj) || fs::last_write_time(src) > fs::last_write_time(obj)) {
            tasks.push_back({src, obj});
        }
    }

    string out_binary = (fs::path(target.output_dir) / (target.name + EXE_EXT)).string();
    bool needs_link = !fs::exists(out_binary);
    if (!needs_link) {
        for (const auto& obj : objects) {
            if (fs::exists(obj) && fs::exists(out_binary) && fs::last_write_time(obj) > fs::last_write_time(out_binary)) {
                needs_link = true;
                break;
            }
        }
    }

    if (!needs_link && tasks.empty()) {
        cout << "build: Target is up to date.\n";
        return;
    }

    string vcvars_bat = find_vcvars_bat_path();
    if (compiler == CompilerType::MSVC && !vcvars_bat.empty()) {
#if defined(PLATFORM_WINDOWS)
        string vcvars_error;
        if (!initialize_msvc_environment(vcvars_bat, vcvars_error)) {
            cerr << "build: error: failed to initialize MSVC environment from " << vcvars_bat << ": " << vcvars_error << "\n";
            return;
        }
#endif
    }

    cout << "build: Compiling " << tasks.size() << " file(s) using " << jobs << " worker thread(s)...\n";
    std::atomic<size_t> idx(0);
    std::atomic<bool> failed(false);

    auto worker = [&]() {
        while (true) {
            size_t i = idx.fetch_add(1);
            if (i >= tasks.size() || failed.load()) break;

            const auto& [src, obj] = tasks[i];
            string cmd;
            if (compiler == CompilerType::MSVC) {
                cmd = "cl.exe /nologo /EHsc /O2 /c " + shell_quote(src) + " /Fo" + shell_quote(obj);
            } else if (compiler == CompilerType::CLANG) {
                cmd = "clang++ -O2 -c " + shell_quote(src) + " -o " + shell_quote(obj);
            } else {
                cmd = "g++ -O2 -c " + shell_quote(src) + " -o " + shell_quote(obj);
            }

            {
                lock_guard<mutex> lock(global_log_mutex);
                cout << "  [" << (i + 1) << "/" << tasks.size() << "] " << src << "\n";
                if (verbose) cout << "    " << cmd << "\n";
            }

            string out;
            if (run_cmd(cmd, out) != 0) {
                lock_guard<mutex> lock(global_log_mutex);
                cerr << "Compilation Error in " << src << ":\n" << out << "\n";
                failed.store(true);
            }
        }
    };

    vector<thread> threads;
    for (int j = 0; j < jobs; ++j) threads.emplace_back(worker);
    for (auto& t : threads) t.join();

    if (failed.load()) return;

    string link_cmd;
    string object_args;
    for (const auto& obj : objects) {
        object_args += " " + shell_quote(obj);
    }
    if (compiler == CompilerType::MSVC) {
        link_cmd = "cl.exe /nologo /Fe" + shell_quote(out_binary) + object_args;
    } else if (compiler == CompilerType::CLANG) {
        link_cmd = "clang++ -o " + shell_quote(out_binary) + object_args;
    } else {
        link_cmd = "g++ -o " + shell_quote(out_binary) + object_args;
    }

    cout << "build: Linking " << out_binary << "...\n";
    string link_out;
    if (run_cmd(link_cmd, link_out) == 0) {
        cout << "build: Successfully built " << out_binary << "\n";
    } else {
        cerr << "Linker Error:\n" << link_out << "\n";
    }
}

} // namespace BuildModule
