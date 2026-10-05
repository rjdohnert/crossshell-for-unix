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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <streambuf>

namespace fs = std::filesystem;

enum class OutputFormat { Human, Json, Csv, Table };
class OutputBuffer : public std::streambuf {
    std::streambuf* target_; OutputFormat format_; std::string pending_; bool first_ = true;
    void emit() { if (pending_.empty()) return; std::string out; if (format_ == OutputFormat::Json) { if (!first_) target_->sputn(",\n", 2); first_ = false; out = "{\"message\":\""; for (char c : pending_) { if (c == '"' || c == '\\') out += '\\'; if (c == '\r') out += "\\r"; else out += c; } out += "\"}"; } else if (format_ == OutputFormat::Csv) { out = "\""; for (char c : pending_) out += c == '"' ? "\"\"" : std::string(1, c); out += "\"\n"; } else out = pending_ + "\n"; target_->sputn(out.data(), static_cast<std::streamsize>(out.size())); pending_.clear(); }
public:
    OutputBuffer(std::streambuf* target, OutputFormat format) : target_(target), format_(format) { if (format_ == OutputFormat::Json) target_->sputn("[\n", 2); else if (format_ == OutputFormat::Table) target_->sputn("MESSAGE\n-------\n", 16); }
    ~OutputBuffer() override { emit(); if (format_ == OutputFormat::Json) target_->sputn("\n]\n", 3); }
    int_type overflow(int_type c) override { if (c != traits_type::eof()) { if (c == '\n') emit(); else pending_.push_back(static_cast<char>(c)); } return traits_type::not_eof(c); }
    int sync() override { emit(); return target_->pubsync(); }
};
class PipeBuffer : public std::streambuf { FILE* file_; char buffer_[4096]; public: explicit PipeBuffer(FILE* f):file_(f){setp(buffer_,buffer_+sizeof(buffer_));} int_type overflow(int_type c) override {if(c!=traits_type::eof()){*pptr()=static_cast<char>(c);pbump(1);}return sync()==0?traits_type::not_eof(c):traits_type::eof();} int sync() override {auto n=pptr()-pbase();if(n)std::fwrite(pbase(),1,static_cast<size_t>(n),file_);setp(buffer_,buffer_+sizeof(buffer_));return std::fflush(file_);} };
class OutputSession { std::streambuf* old_; FILE* file_=nullptr; PipeBuffer* pipe_=nullptr; OutputBuffer* output_=nullptr; public: OutputSession(OutputFormat f,const std::string& cmd):old_(std::cout.rdbuf()){if(f==OutputFormat::Human&&cmd.empty())return;std::streambuf* target=old_;if(!cmd.empty()&&(file_=_popen(cmd.c_str(),"w"))){pipe_=new PipeBuffer(file_);target=pipe_;}output_=new OutputBuffer(target,f);std::cout.rdbuf(output_);} ~OutputSession(){if(!output_)return;std::cout.flush();std::cout.rdbuf(old_);delete output_;delete pipe_;if(file_)_pclose(file_);} };

struct ZipOptions {
    bool recursive = false;
    bool junk_paths = false;
    bool quiet = false;
    bool force = false;
    int compression_level = 6;
    std::string zip_filename;
    std::vector<std::string> input_paths;
    std::vector<std::string> exclude_paths;
    OutputFormat output_format = OutputFormat::Human;
    std::string pipe_command;
};

std::string escape_ps_single_quotes(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\'') {
            out += "''";
        } else {
            out += c;
        }
    }
    return out;
}

std::string ps_quote(const std::string& s) {
    return "'" + escape_ps_single_quotes(s) + "'";
}

std::string compression_level_to_ps(int level) {
    if (level <= 0) return "NoCompression";
    if (level <= 3) return "Fastest";
    return "Optimal";
}

bool is_zip_ext(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext == ".zip";
}

void print_usage(const char* prog_name) {
    std::cout << R"(zip(1)                  CrossShell for UNIX Reference Manual                   zip(1)

    NAME
        zip - package and compress (archive) files into ZIP format

    SYNOPSIS
        zip [OPTIONS] ARCHIVE [FILE...]

    DESCRIPTION
        zip is a compression and file packaging utility for Windows NTFS/ReFS
        and FAT systems.

    OPTIONS
        -r, --recurse-paths
            Travel the directory structure recursively.

        -j, --junk-paths
            Store just names of saved files (junk the paths).

        -q, --quiet
            Quiet mode; eliminate informational messages.

        -0..-9
            Compression level (0=store, 9=best compression, 6=default).

        -f, --freshen
            Freshen existing archive entries only.

        -x PATTERN
            Exclude files matching pattern.

        --json, --csv, --table
            Output archive creation summary as JSON, CSV, or table.

        --pipe COMMAND
            Send output into COMMAND.

        --help
            Display this reference manual.

    EXAMPLES
        zip -r backup.zip src/ docs/
            Recursively package src and docs into backup.zip.

    CrossShell for UNIX                                                    zip(1)
)";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    ZipOptions opts;
    bool parsing_flags = true;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--json" || arg == "--csv" || arg == "--table") { opts.output_format = arg == "--json" ? OutputFormat::Json : (arg == "--csv" ? OutputFormat::Csv : OutputFormat::Table); continue; }
        if (arg == "--pipe" && i + 1 < argc) { opts.pipe_command = argv[++i]; continue; }

        if (parsing_flags && arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }

        if (parsing_flags && arg[0] == '-' && arg.length() > 1) {
            if (arg == "-f") {
                opts.force = true;
            } else if (arg == "-x" && i + 1 < argc) {
                opts.exclude_paths.push_back(argv[++i]);
            } else {
                for (size_t j = 1; j < arg.length(); ++j) {
                    char c = arg[j];
                    if (c == 'r') opts.recursive = true;
                    else if (c == 'j') opts.junk_paths = true;
                    else if (c == 'q') opts.quiet = true;
                    else if (c == 'f') opts.force = true;
                    else if (c >= '0' && c <= '9') opts.compression_level = c - '0';
                    else if (c == '-') { parsing_flags = false; break; }
                    else {
                        std::cerr << "zip error: Invalid option -" << c << "\n";
                        return 1;
                    }
                }
            }
        } else {
            if (opts.zip_filename.empty()) {
                opts.zip_filename = arg;
                // Auto-append .zip extension if missing
                if (!is_zip_ext(fs::path(opts.zip_filename))) {
                    opts.zip_filename += ".zip";
                }
            } else {
                opts.input_paths.push_back(arg);
            }
        }
    }

    if (opts.zip_filename.empty() || opts.input_paths.empty()) {
        std::cerr << "zip error: Nothing to do! (must specify zipfile and input files)\n";
        return 1;
    }

    OutputSession output_session(opts.output_format, opts.pipe_command);

    std::vector<fs::path> archive_inputs;
    std::vector<fs::path> files_for_junk_mode;
    size_t files_added = 0;

    for (const auto& input_str : opts.input_paths) {
        fs::path p(input_str);

        bool excluded = false;
        for (const auto& ex : opts.exclude_paths) {
            if (p.string() == ex || p.filename().string() == ex) {
                excluded = true;
                break;
            }
        }
        if (excluded) continue;

        if (!fs::exists(p)) {
            std::cerr << "zip warning: name not matched: " << input_str << "\n";
            continue;
        }

        if (fs::is_directory(p)) {
            if (!opts.recursive) {
                std::cerr << "zip warning: " << input_str << " is a directory (use -r to recurse)\n";
                continue;
            }

            archive_inputs.push_back(p);
            for (const auto& entry : fs::recursive_directory_iterator(p)) {
                if (entry.is_regular_file()) {
                    files_added++;
                    files_for_junk_mode.push_back(entry.path());
                }
            }
            continue;
        }

        if (fs::is_regular_file(p)) {
            archive_inputs.push_back(p);
            files_for_junk_mode.push_back(p);
            files_added++;
        }
    }

    if (archive_inputs.empty() || files_added == 0) {
        std::cerr << "zip error: Nothing to do!\n";
        return 1;
    }

    fs::path temp_stage_dir;
    std::vector<fs::path> ps_inputs;

    if (opts.junk_paths) {
        temp_stage_dir = fs::temp_directory_path() /
            (std::string("cmdext-zip-stage-") + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));

        try {
            fs::create_directories(temp_stage_dir);

            for (const auto& src : files_for_junk_mode) {
                fs::path candidate = temp_stage_dir / src.filename();
                int idx = 1;
                while (fs::exists(candidate)) {
                    candidate = temp_stage_dir /
                        (src.stem().string() + "_" + std::to_string(idx++) + src.extension().string());
                }
                fs::copy_file(src, candidate, fs::copy_options::overwrite_existing);
                ps_inputs.push_back(candidate);

                if (!opts.quiet) {
                    std::cout << "  adding: " << candidate.filename().string();
                    if (opts.compression_level == 0) {
                        std::cout << " (stored)\n";
                    } else {
                        std::cout << " (deflated)\n";
                    }
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "zip error: Failed preparing junk-path staging area: " << ex.what() << "\n";
            if (!temp_stage_dir.empty()) {
                std::error_code ec;
                fs::remove_all(temp_stage_dir, ec);
            }
            return 1;
        }
    } else {
        ps_inputs = archive_inputs;
        if (!opts.quiet) {
            for (const auto& in : ps_inputs) {
                std::cout << "  adding: " << in.string();
                if (opts.compression_level == 0) {
                    std::cout << " (stored)\n";
                } else {
                    std::cout << " (deflated)\n";
                }
            }
        }
    }

    std::ostringstream path_array;
    path_array << "@(";
    for (size_t i = 0; i < ps_inputs.size(); ++i) {
        if (i != 0) path_array << ",";
        path_array << ps_quote(ps_inputs[i].string());
    }
    path_array << ")";

    std::string psCommand =
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        "$ErrorActionPreference='Stop'; "
        "Compress-Archive -Path " + path_array.str() +
        " -DestinationPath " + ps_quote(fs::absolute(opts.zip_filename).string()) +
        " -CompressionLevel " + compression_level_to_ps(opts.compression_level) +
        " -Force\"";

    int rc = std::system(psCommand.c_str());

    if (!temp_stage_dir.empty()) {
        std::error_code ec;
        fs::remove_all(temp_stage_dir, ec);
    }

    if (rc != 0) {
        std::cerr << "zip error: Failed to create archive via PowerShell Compress-Archive.\n";
        return 1;
    }

    if (!opts.quiet) {
        std::cout << "Created " << opts.zip_filename << " (" << files_added << " file(s) added)\n";
    }

    return 0;
}
