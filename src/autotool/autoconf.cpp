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

#include "autoconf.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "help.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;
using std::string;
using std::vector;
using std::map;
using std::pair;
using std::cout;
using std::cerr;
using std::ifstream;
using std::ofstream;
using std::ios;

namespace AutoconfModule {

class ProbeEngine {
    string compiler;
    string cflags;
    ofstream log;

    static bool is_msvc_like(const string& value) {
        string lower = value;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(tolower(c));
        });
        return lower.find("cl") != string::npos || lower.find("clang-cl") != string::npos;
    }

    string probe_obj_path() const {
        return is_msvc_like(compiler) ? "conftest.obj" : "conftest.o";
    }

    string build_probe_command(const string& obj_path) const {
        string cmd = shell_quote(compiler);
        if (!cflags.empty()) {
            cmd += " " + cflags;
        }
        if (is_msvc_like(compiler)) {
            cmd += " /c conftest.c /Fo" + shell_quote(obj_path);
        } else {
            cmd += " -c conftest.c -o " + shell_quote(obj_path);
        }
        return cmd + " >conftest.out 2>&1";
    }

public:
    ProbeEngine(const string& comp, const string& flags, const string& log_file) : compiler(comp), cflags(flags) {
        log.open(log_file, ios::app);
    }

    bool compile_check(const string& src) {
        ofstream out("conftest.c");
        out << src;
        out.close();

        string obj_path = probe_obj_path();
        string cmd = build_probe_command(obj_path);
        string output;
        int res = run_cmd(cmd, output);

        ifstream err("conftest.out");
        log << "Compiler Output:\n" << err.rdbuf() << "\nExit Code: " << res << "\n";

        fs::remove("conftest.c");
        fs::remove(obj_path);
        fs::remove("conftest.out");
        return (res == 0);
    }

    bool check_header(const string& hdr) {
        return compile_check("#include <" + hdr + ">\nint main() { return 0; }\n");
    }

    bool check_func(const string& func) {
        return compile_check("char " + func + "();\nint main() { return " + func + "(); }\n");
    }
};

void run_autoconf(const vector<string>& args) {
    bool execute_now = false;
    string input_file = "configure.ac";
    string prefix = "C:\\Program Files\\Package";
    string cc = "cl.exe";
    string cflags = "/O2 /nologo";
    string cxx = "cl.exe";
    string cxxflags = "/O2 /nologo /EHsc";
    string ldflags = "";
    string libs = "";
    CompilerType detected = detect_compiler();
    if (detected == CompilerType::GCC) {
        cc = "gcc";
        cflags = "-O2";
        cxx = "g++";
        cxxflags = "-O2";
    } else if (detected == CompilerType::CLANG) {
        cc = "clang";
        cflags = "-O2";
        cxx = "clang++";
        cxxflags = "-O2";
    } else if (detected == CompilerType::XLC) {
        cc = "xlc";
        cflags = "-O2";
        cxx = "xlc++";
        cxxflags = "-O2";
    }

    for (size_t i = 0; i < args.size(); ++i) {
        string arg = args[i];
        if (arg == "-h" || arg == "--help") { print_autoconf_help(); return; }
        else if (arg == "-r" || arg == "--run") execute_now = true;
        else if (arg.rfind("--prefix=", 0) == 0) prefix = arg.substr(9);
        else if (arg.rfind("CC=", 0) == 0) cc = arg.substr(3);
        else if (arg.rfind("CXX=", 0) == 0) cxx = arg.substr(4);
        else if (arg.rfind("CFLAGS=", 0) == 0) cflags = arg.substr(7);
        else if (arg.rfind("CXXFLAGS=", 0) == 0) cxxflags = arg.substr(9);
        else if (arg.rfind("LDFLAGS=", 0) == 0) ldflags = arg.substr(8);
        else if (arg.rfind("LIBS=", 0) == 0) libs = arg.substr(5);
        else if (arg[0] != '-') input_file = arg;
    }

    if (!fs::exists(input_file)) {
        cerr << "autoconf: error: cannot find input file '" << input_file << "'\n";
        return;
    }

    AutoconfProject proj;
    ifstream file(input_file);
    string line;
    while (getline(file, line)) {
        string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        string macro_name;
        vector<string> macro_args;
        if (!parse_m4_macro_call(trimmed, macro_name, macro_args)) {
            continue;
        }

        if (macro_name == "AC_INIT") {
            if (!macro_args.empty()) proj.package_name = unquote_m4(macro_args[0]);
            if (macro_args.size() >= 2) proj.package_version = unquote_m4(macro_args[1]);
        } else if (macro_name == "AC_PROG_CC") {
            proj.check_cc = true;
        } else if (macro_name == "AC_PROG_CXX") {
            proj.check_cxx = true;
        } else if (macro_name == "AC_CHECK_HEADERS") {
            proj.check_headers = macro_args;
        } else if (macro_name == "AC_CHECK_FUNCS") {
            proj.check_funcs = macro_args;
        } else if (macro_name == "AC_CHECK_TYPES") {
            proj.check_types = macro_args;
        } else if (macro_name == "AC_CHECK_SIZEOF") {
            proj.check_sizeof = macro_args;
        } else if (macro_name == "AC_CHECK_DECLS") {
            proj.check_decls = macro_args;
        } else if (macro_name == "AC_CHECK_MEMBERS") {
            proj.check_members = macro_args;
        } else if (macro_name == "AC_SEARCH_LIBS") {
            if (macro_args.size() >= 2) {
                proj.search_libs.push_back({macro_args[0], vector<string>(macro_args.begin() + 1, macro_args.end())});
            }
        } else if (macro_name == "AC_CONFIG_SRCDIR") {
            if (!macro_args.empty()) proj.src_dirs.push_back(unquote_m4(macro_args[0]));
        } else if (macro_name == "AC_CONFIG_AUX_DIR") {
            if (!macro_args.empty()) proj.aux_dirs.push_back(unquote_m4(macro_args[0]));
        } else if (macro_name == "AC_SUBST_FILE") {
            if (!macro_args.empty()) proj.subst_files.push_back(unquote_m4(macro_args[0]));
        } else if (macro_name == "AC_SUBST") {
            if (!macro_args.empty()) {
                string name = unquote_m4(macro_args[0]);
                string value = macro_args.size() >= 2 ? unquote_m4(macro_args[1]) : "";
                proj.subst_names.push_back(name);
                proj.subst_values[name] = value;
            }
        } else if (macro_name == "AC_DEFINE") {
            if (!macro_args.empty()) {
                string name = unquote_m4(macro_args[0]);
                string value = macro_args.size() >= 2 ? unquote_m4(macro_args[1]) : "1";
                proj.defines[name] = value;
            }
        } else if (macro_name == "AC_CONFIG_FILES") {
            for (const auto& arg : macro_args) {
                auto parts = split_words(arg);
                proj.config_files.insert(proj.config_files.end(), parts.begin(), parts.end());
            }
        } else if (macro_name == "AC_OUTPUT") {
            for (const auto& arg : macro_args) {
                auto parts = split_words(arg);
                proj.output_files.insert(proj.output_files.end(), parts.begin(), parts.end());
            }
        } else if (macro_name == "AC_CONFIG_HEADERS") {
            proj.config_headers = macro_args;
        }
    }

    if (execute_now) {
        cout << "configure: configuring " << proj.package_name << " " << proj.package_version << "...\n";
        ProbeEngine tester(cc, cflags, "config.log");

        proj.subst_vars["PACKAGE_NAME"] = proj.package_name;
        proj.subst_vars["PACKAGE_VERSION"] = proj.package_version;
        proj.subst_vars["PACKAGE"] = proj.package_name;
        proj.subst_vars["VERSION"] = proj.package_version;
        proj.subst_vars["prefix"] = prefix;
        proj.subst_vars["CC"] = cc;
        proj.subst_vars["CXX"] = cxx;
        proj.subst_vars["CFLAGS"] = cflags;
        proj.subst_vars["CXXFLAGS"] = cxxflags;
        proj.subst_vars["LDFLAGS"] = ldflags;
        proj.subst_vars["LIBS"] = libs;
        proj.subst_vars["srcdir"] = ".";
        proj.subst_vars["builddir"] = ".";

        for (const auto& name : proj.subst_names) {
            if (proj.subst_vars.count(name)) continue;
            if (proj.subst_values.count(name)) {
                string value = proj.subst_values[name];
                for (const auto& [src, dst] : proj.subst_vars) {
                    value = replace_all(value, "@" + src + "@", dst);
                    value = replace_all(value, "$" + string("{") + src + "}", dst);
                }
                for (const auto& [define_name, define_value] : proj.defines) {
                    value = replace_all(value, "@" + define_name + "@", define_value);
                    value = replace_all(value, "$" + string("{") + define_name + "}", define_value);
                }
                proj.subst_vars[name] = value;
            } else if (name == "PACKAGE_NAME") proj.subst_vars[name] = proj.package_name;
            else if (name == "PACKAGE_VERSION") proj.subst_vars[name] = proj.package_version;
            else if (name == "PACKAGE") proj.subst_vars[name] = proj.package_name;
            else if (name == "VERSION") proj.subst_vars[name] = proj.package_version;
            else if (name == "prefix") proj.subst_vars[name] = prefix;
            else if (name == "CC") proj.subst_vars[name] = cc;
            else if (name == "CXX") proj.subst_vars[name] = cxx;
            else if (name == "CFLAGS") proj.subst_vars[name] = cflags;
            else if (name == "CXXFLAGS") proj.subst_vars[name] = cxxflags;
            else if (name == "LDFLAGS") proj.subst_vars[name] = ldflags;
            else if (name == "LIBS") proj.subst_vars[name] = libs;
            else proj.subst_vars[name] = "";
        }

        for (const auto& h : proj.check_headers) {
            cout << "checking for " << h << "... ";
            bool res = tester.check_header(h);
            cout << (res ? "yes" : "no") << "\n";
            if (res) proj.defines["HAVE_" + to_upper_identifier(h)] = "1";
        }

        for (auto& [name, value] : proj.subst_values) {
            if (value.find("@") != string::npos || value.find("${") != string::npos) {
                string expanded = value;
                for (const auto& [src, dst] : proj.subst_vars) {
                    expanded = replace_all(expanded, "@" + src + "@", dst);
                    expanded = replace_all(expanded, "$" + string("{") + src + "}", dst);
                }
                for (const auto& [define_name, define_value] : proj.defines) {
                    expanded = replace_all(expanded, "@" + define_name + "@", define_value);
                    expanded = replace_all(expanded, "$" + string("{") + define_name + "}", define_value);
                }
                proj.subst_vars[name] = expanded;
            }
        }

        for (const auto& f : proj.check_funcs) {
            cout << "checking for " << f << "... ";
            bool res = tester.check_func(f);
            cout << (res ? "yes" : "no") << "\n";
            if (res) proj.defines["HAVE_" + to_upper_identifier(f)] = "1";
        }

        for (const auto& type_name : proj.check_types) {
            cout << "checking for type " << type_name << "... ";
            bool res = tester.compile_check("typedef " + type_name + " test_type;\nint main() { return 0; }\n");
            cout << (res ? "yes" : "no") << "\n";
            if (res) proj.defines["HAVE_" + to_upper_identifier(type_name)] = "1";
        }

        for (const auto& size_name : proj.check_sizeof) {
            cout << "checking size of " << size_name << "... ";
            bool res = tester.compile_check("#include <stddef.h>\ntypedef " + size_name + " test_type;\nint main() { return sizeof(test_type); }\n");
            cout << (res ? "yes" : "no") << "\n";
            if (res) proj.defines["SIZEOF_" + to_upper_identifier(size_name)] = "1";
        }

        for (const auto& decl_name : proj.check_decls) {
            cout << "checking for declaration " << decl_name << "... ";
            bool res = tester.compile_check("#include <stdio.h>\nint main() { (void)" + decl_name + "; return 0; }\n");
            cout << (res ? "yes" : "no") << "\n";
            if (res) proj.defines["HAVE_DECL_" + to_upper_identifier(decl_name)] = "1";
        }

        for (const auto& member_name : proj.check_members) {
            cout << "checking for member " << member_name << "... ";
            bool res = tester.compile_check("struct test_struct { int field; };\nint main() { return sizeof(((test_struct*)0)->field); }\n");
            cout << (res ? "yes" : "no") << "\n";
            if (res) proj.defines["HAVE_" + to_upper_identifier(member_name)] = "1";
        }

        for (const auto& [symbol, libs_vec] : proj.search_libs) {
            cout << "checking for library for " << symbol << "... ";
            bool res = false;
            for (const auto& lib : libs_vec) {
                if (tester.compile_check("extern int " + symbol + "();\nint main() { return " + symbol + "(); }\n")) {
                    res = true;
                    break;
                }
            }
            cout << (res ? "yes" : "no") << "\n";
            if (res) proj.defines["HAVE_" + to_upper_identifier(symbol)] = "1";
        }

        for (const auto& path : proj.aux_dirs) {
            fs::create_directories(path);
        }
        for (const auto& path : proj.src_dirs) {
            fs::create_directories(path);
        }
        for (const auto& file_path : proj.subst_files) {
            if (fs::exists(file_path)) {
                std::ifstream in(file_path);
                std::ofstream out(file_path + ".out");
                std::string l;
                while (std::getline(in, l)) {
                    out << expand_m4_template(l, proj.subst_vars) << "\n";
                }
            }
        }

        for (const auto& hdr_file : proj.config_headers.empty() ? vector<string>{"config.h"} : proj.config_headers) {
            ofstream out(hdr_file);
            out << "#ifndef CONFIG_H_INCLUDED\n#define CONFIG_H_INCLUDED\n\n";
            for (const auto& [k, v] : proj.defines) out << "#define " << k << " " << v << "\n";
            out << "\n#endif\n";
            cout << "config.status: creating " << hdr_file << "\n";
        }

        auto emit_templates = [&](const vector<string>& files) {
            for (const auto& cfg : files) {
                string in_path = cfg;
                string out_path = cfg;
                size_t sep = cfg.find(':');
                if (sep != string::npos) {
                    out_path = cfg.substr(0, sep);
                    in_path = cfg.substr(sep + 1);
                }
                if (in_path.empty()) in_path = out_path + ".in";
                fs::path output_path = fs::path(out_path);
                if (output_path.has_parent_path() && !output_path.parent_path().empty()) {
                    fs::create_directories(output_path.parent_path());
                }
                if (fs::exists(in_path)) {
                    ifstream in(in_path);
                    ofstream out(out_path);
                    string l;
                    while (getline(in, l)) {
                        out << expand_m4_template(l, proj.subst_vars) << "\n";
                    }
                    cout << "config.status: creating " << out_path << "\n";
                } else if (fs::exists(out_path)) {
                    cout << "config.status: updating " << out_path << "\n";
                } else {
                    ofstream out(out_path);
                    out << "";
                    cout << "config.status: creating " << out_path << "\n";
                }
            }
        };

        emit_templates(proj.config_files);
        emit_templates(proj.output_files);
    } else {
        ofstream cmd("configure.cmd");
        cmd << "@echo off\n" << TOOL_NAME << ".exe autoconf --run %*\n";
        cout << "autoconf: created native launcher (configure.cmd)\n";
    }
}

} // namespace AutoconfModule
