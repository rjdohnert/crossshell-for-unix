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

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <streambuf>

namespace fs = std::filesystem;

enum class OutputFormat { Human, Json, Csv, Table };
class OutputBuffer : public std::streambuf { std::streambuf* target_; OutputFormat format_; std::string pending_; bool first_=true; void emit(){if(pending_.empty())return;std::string out;if(format_==OutputFormat::Json){if(!first_)target_->sputn(",\n",2);first_=false;out="{\"message\":\"";for(char c:pending_){if(c=='"'||c=='\\')out+='\\';if(c=='\r')out+="\\r";else out+=c;}out+="\"}";}else if(format_==OutputFormat::Csv){out="\"";for(char c:pending_)out+=c=='"'?"\"\"":std::string(1,c);out+="\"\n";}else out=pending_+"\n";target_->sputn(out.data(),static_cast<std::streamsize>(out.size()));pending_.clear();} public: OutputBuffer(std::streambuf*t,OutputFormat f):target_(t),format_(f){if(f==OutputFormat::Json)target_->sputn("[\n",2);else if(f==OutputFormat::Table)target_->sputn("MESSAGE\n-------\n",16);}~OutputBuffer()override{emit();if(format_==OutputFormat::Json)target_->sputn("\n]\n",3);}int_type overflow(int_type c)override{if(c!=traits_type::eof()){if(c=='\n')emit();else pending_+=static_cast<char>(c);}return traits_type::not_eof(c);}int sync()override{emit();return target_->pubsync();}};
class PipeBuffer : public std::streambuf { FILE* file_; char buffer_[4096]; public: explicit PipeBuffer(FILE*f):file_(f){setp(buffer_,buffer_+sizeof(buffer_));}int_type overflow(int_type c)override{if(c!=traits_type::eof()){*pptr()=static_cast<char>(c);pbump(1);}return sync()==0?traits_type::not_eof(c):traits_type::eof();}int sync()override{auto n=pptr()-pbase();if(n)std::fwrite(pbase(),1,static_cast<size_t>(n),file_);setp(buffer_,buffer_+sizeof(buffer_));return std::fflush(file_);}};
class OutputSession { std::streambuf* old_;FILE*file_=nullptr;PipeBuffer*pipe_=nullptr;OutputBuffer*out_=nullptr;public:OutputSession(OutputFormat f,const std::string&cmd):old_(std::cout.rdbuf()){if(f==OutputFormat::Human&&cmd.empty())return;std::streambuf*t=old_;if(!cmd.empty()&&(file_=_popen(cmd.c_str(),"w"))){pipe_=new PipeBuffer(file_);t=pipe_;}out_=new OutputBuffer(t,f);std::cout.rdbuf(out_);}~OutputSession(){if(!out_)return;std::cout.flush();std::cout.rdbuf(old_);delete out_;delete pipe_;if(file_)_pclose(file_);}};

struct UnzipOptions {
    bool list = false;
    bool test = false;
    bool quiet = false;
    bool overwrite = false;
    bool never_overwrite = false;
    bool junk_paths = false;
    std::string dest_dir = ".";
    std::string zip_filename;
    std::vector<std::string> filters;
    OutputFormat output_format = OutputFormat::Human;
    std::string pipe_command;
};

bool simple_match(const std::string& pattern, const std::string& str) {
    if (pattern == str) return true;

    size_t star = pattern.find('*');
    if (star != std::string::npos) {
        std::string prefix = pattern.substr(0, star);
        std::string suffix = pattern.substr(star + 1);
        if (str.length() >= prefix.length() + suffix.length()) {
            return str.compare(0, prefix.length(), prefix) == 0 &&
                   str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
        }
    }

    return false;
}

bool should_extract(const std::string& filename, const std::vector<std::string>& filters) {
    if (filters.empty()) return true;
    for (const auto& filter : filters) {
        if (simple_match(filter, filename)) return true;
    }
    return false;
}

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

std::string run_capture(const std::string& command, int& exitCode) {
    std::string out;

    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) {
        exitCode = -1;
        return out;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        out += buffer;
    }

    exitCode = _pclose(pipe);
    return out;
}

std::string make_temp_file(const std::string& prefix) {
    fs::path p = fs::temp_directory_path() /
        (prefix + "-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()) + ".txt");
    return p.string();
}

void print_usage(const char* prog_name) {
    std::cout << R"(unzip(1)                CrossShell for UNIX Reference Manual                 unzip(1)

    NAME
        unzip - list, test and extract compressed files in a ZIP archive

    SYNOPSIS
        unzip [OPTIONS] FILE[.zip] [FILE...] [-d EXDIR]

    DESCRIPTION
        unzip will list, test, or extract files from a ZIP archive.

    OPTIONS
        -l
            List archive contents in short format.

        -t
            Test archive files integrity.

        -d EXDIR
            Extract files into directory EXDIR.

        -o
            Overwrite existing files without prompting.

        -f
            Freshen existing files (overwrite only if newer).

        -n
            Never overwrite existing files.

        -j
            Junk paths (extract all files flat into target directory).

        -q
            Quiet mode.

        --json, --csv, --table
            Format archive entry manifests as JSON, CSV, or table.

        --pipe COMMAND
            Stream extraction report to COMMAND.

        --help
            Display this reference manual.

    EXAMPLES
        unzip archive.zip -d out/
            Extract archive into out/ directory.

        unzip -l archive.zip --json
            List contents of archive in JSON format.

    CrossShell for UNIX                                                  unzip(1)
)";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    UnzipOptions opts;
    bool parsing_flags = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--json" || arg == "--csv" || arg == "--table") { opts.output_format = arg == "--json" ? OutputFormat::Json : (arg == "--csv" ? OutputFormat::Csv : OutputFormat::Table); continue; }
        if (arg == "--pipe" && i + 1 < argc) { opts.pipe_command = argv[++i]; continue; }

        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-d") {
            if (i + 1 < argc) {
                opts.dest_dir = argv[++i];
            } else {
                std::cerr << "unzip error: -d option requires a directory argument\n";
                return 1;
            }
        } else if (arg.rfind("-d", 0) == 0 && arg.length() > 2) {
            opts.dest_dir = arg.substr(2);
        } else if (parsing_flags && arg[0] == '-' && arg.length() > 1) {
            for (size_t j = 1; j < arg.length(); ++j) {
                char c = arg[j];
                if (c == 'l') opts.list = true;
                else if (c == 't') opts.test = true;
                else if (c == 'q') opts.quiet = true;
                else if (c == 'o') opts.overwrite = true;
                else if (c == 'f') {
                    opts.overwrite = true;
                }
                else if (c == 'n') opts.never_overwrite = true;
                else if (c == 'j') opts.junk_paths = true;
                else if (c == '-') { parsing_flags = false; break; }
                else {
                    std::cerr << "unzip error: Unknown option -" << c << "\n";
                    return 1;
                }
            }
        } else {
            if (opts.zip_filename.empty()) {
                opts.zip_filename = arg;
                if (!fs::exists(opts.zip_filename)) {
                    fs::path p(opts.zip_filename);
                    std::string ext = p.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
                        return static_cast<char>(std::tolower(ch));
                    });
                    if (ext != ".zip" && fs::exists(opts.zip_filename + ".zip")) {
                        opts.zip_filename += ".zip";
                    }
                }
            } else {
                opts.filters.push_back(arg);
            }
        }
    }

    if (opts.zip_filename.empty()) {
        std::cerr << "unzip error: No zipfile specified.\n";
        return 1;
    }

    OutputSession output_session(opts.output_format, opts.pipe_command);

    fs::path zipPath = fs::absolute(opts.zip_filename);
    if (!fs::exists(zipPath)) {
        std::cerr << "unzip error: Cannot open " << opts.zip_filename << "\n";
        return 1;
    }

    std::string listFile = make_temp_file("cmdext-unzip-list");
    std::string psListCmd =
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        "$ErrorActionPreference='Stop'; "
        "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
        "$zip=[IO.Compression.ZipFile]::OpenRead(" + ps_quote(zipPath.string()) + "); "
        "foreach($e in $zip.Entries){ "
        "  if($e.FullName.EndsWith('/')){continue}; "
        "  $dt=$e.LastWriteTime.UtcDateTime.ToString('yyyy-MM-dd HH:mm'); "
        "  Write-Output ($e.Length.ToString() + '\t' + $dt + '\t' + $e.FullName)"
        "}; "
        "$zip.Dispose()\"";

    int psExit = 0;
    std::string listOutput = run_capture(psListCmd, psExit);
    if (psExit != 0) {
        std::cerr << "unzip error: Failed to read zip archive metadata.\n";
        return 1;
    }

    {
        std::ofstream ofs(listFile, std::ios::binary);
        ofs << listOutput;
    }

    struct Entry {
        uint64_t size = 0;
        std::string date;
        std::string name;
    };

    std::vector<Entry> entries;
    {
        std::ifstream ifs(listFile, std::ios::binary);
        std::string line;
        while (std::getline(ifs, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            size_t t1 = line.find('\t');
            size_t t2 = (t1 == std::string::npos) ? std::string::npos : line.find('\t', t1 + 1);
            if (t1 == std::string::npos || t2 == std::string::npos) continue;

            Entry e;
            try {
                e.size = static_cast<uint64_t>(std::stoull(line.substr(0, t1)));
            } catch (...) {
                continue;
            }
            e.date = line.substr(t1 + 1, t2 - (t1 + 1));
            e.name = line.substr(t2 + 1);

            if (should_extract(e.name, opts.filters)) {
                entries.push_back(std::move(e));
            }
        }
    }

    std::error_code ecRemove;
    fs::remove(listFile, ecRemove);

    if (opts.list) {
        std::cout << "Archive:  " << opts.zip_filename << "\n";
        std::cout << "  Length      Date    Time    Name\n";
        std::cout << "---------  ---------- -----   -----\n";

        uint64_t total_size = 0;
        uint64_t total_files = 0;

        for (const auto& e : entries) {
            std::string date = e.date;
            std::string d = date.size() >= 10 ? date.substr(0, 10) : "          ";
            std::string t = date.size() >= 16 ? date.substr(11, 5) : "     ";

            std::cout << std::setw(9) << e.size << "  "
                      << d << " " << t << "   "
                      << e.name << "\n";

            total_size += e.size;
            total_files++;
        }

        std::cout << "---------                     -------\n";
        std::cout << std::setw(9) << total_size << "                     " << total_files << " file(s)\n";
        return 0;
    }

    if (opts.test) {
        if (!opts.quiet) std::cout << "Archive:  " << opts.zip_filename << "\n";

        std::string testCmd =
            "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
            "$ErrorActionPreference='Stop'; "
            "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
            "$zip=[IO.Compression.ZipFile]::OpenRead(" + ps_quote(zipPath.string()) + "); "
            "$ok=$true; "
            "foreach($e in $zip.Entries){ "
            "  if($e.FullName.EndsWith('/')){continue}; "
            "  $s=$null; "
            "  try{ $s=$e.Open(); $buf=New-Object byte[] 65536; while(($n=$s.Read($buf,0,$buf.Length)) -gt 0){}; "
            "       Write-Output ('OK\t' + $e.FullName) }"
            "  catch{ Write-Output ('FAIL\t' + $e.FullName); $ok=$false }"
            "  finally{ if($s){$s.Dispose()} }"
            "}; "
            "$zip.Dispose(); if(-not $ok){exit 1}\"";

        int testExit = 0;
        std::string testOut = run_capture(testCmd, testExit);

        std::istringstream iss(testOut);
        std::string line;
        bool all_ok = (testExit == 0);
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            size_t tab = line.find('\t');
            if (tab == std::string::npos) continue;

            std::string status = line.substr(0, tab);
            std::string name = line.substr(tab + 1);
            if (!should_extract(name, opts.filters)) continue;

            if (!opts.quiet) {
                if (status == "OK") {
                    std::cout << "    testing: " << name << "   OK\n";
                } else {
                    std::cout << "    testing: " << name << "   FAILED\n";
                }
            }

            if (status != "OK") all_ok = false;
        }

        if (!all_ok) {
            std::cerr << "Errors detected in " << opts.zip_filename << "!\n";
            return 1;
        }

        if (!opts.quiet) {
            std::cout << "No errors detected in compressed data of " << opts.zip_filename << ".\n";
        }
        return 0;
    }

    if (!opts.quiet) {
        std::cout << "Archive:  " << opts.zip_filename << "\n";
    }

    fs::path destDir = fs::absolute(opts.dest_dir);
    std::error_code ec;
    fs::create_directories(destDir, ec);
    if (ec) {
        std::cerr << "unzip error: Could not create destination directory: " << opts.dest_dir << "\n";
        return 1;
    }

    bool overwrite_all = opts.overwrite;
    bool skip_all = opts.never_overwrite;

    std::vector<std::string> selectedNames;
    selectedNames.reserve(entries.size());
    for (const auto& e : entries) {
        selectedNames.push_back(e.name);
    }

    std::string namesArray = "@(";
    for (size_t i = 0; i < selectedNames.size(); ++i) {
        if (i != 0) namesArray += ",";
        namesArray += ps_quote(selectedNames[i]);
    }
    namesArray += ")";

    std::string psExtractCmd =
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        "$ErrorActionPreference='Stop'; "
        "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
        "$zip=[IO.Compression.ZipFile]::OpenRead(" + ps_quote(zipPath.string()) + "); "
        "$sel=@{}; foreach($n in " + namesArray + "){ $sel[$n]=$true }; "
        "$out=@(); "
        "foreach($e in $zip.Entries){ "
        "  if($e.FullName.EndsWith('/')){continue}; "
        "  if(-not $sel.ContainsKey($e.FullName)){continue}; "
        "  $out += $e.FullName "
        "}; "
        "$out | ForEach-Object { Write-Output $_ }; "
        "$zip.Dispose()\"";

    int selExit = 0;
    std::string selOut = run_capture(psExtractCmd, selExit);
    if (selExit != 0) {
        std::cerr << "unzip error: Failed to enumerate selected entries from archive.\n";
        return 1;
    }

    std::vector<std::string> entriesToProcess;
    {
        std::istringstream iss(selOut);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) entriesToProcess.push_back(line);
        }
    }

    for (const auto& rawName : entriesToProcess) {
        fs::path rel(rawName);
        fs::path entryName = opts.junk_paths ? rel.filename() : rel;
        fs::path targetPath = destDir / entryName;
        fs::path targetNorm = fs::weakly_canonical(targetPath.parent_path(), ec) / targetPath.filename();

        fs::path destNorm = fs::weakly_canonical(destDir, ec);
        auto mismatchPair = std::mismatch(destNorm.begin(), destNorm.end(), targetNorm.begin());
        if (mismatchPair.first != destNorm.end()) {
            std::cerr << "unzip warning: Skipping insecure path: " << rawName << "\n";
            continue;
        }

        fs::create_directories(targetPath.parent_path(), ec);

        if (fs::exists(targetPath)) {
            if (skip_all) {
                continue;
            }

            if (!overwrite_all) {
                std::cout << "replace " << targetPath.string() << "? [y]es, [n]o, [A]ll, [N]one: ";
                std::string resp;
                std::cin >> resp;

                if (resp == "y" || resp == "Y") {
                } else if (resp == "n" || resp == "N") {
                    continue;
                } else if (resp == "A" || resp == "a") {
                    overwrite_all = true;
                } else if (resp == "N" || resp == "none") {
                    skip_all = true;
                    continue;
                } else {
                    continue;
                }
            }
        }

        if (!opts.quiet) {
            std::cout << "  inflating: " << targetPath.string() << "\n";
        }

        std::string tmpExtractDir = make_temp_file("cmdext-unzip-stage");
        fs::remove(tmpExtractDir, ec);
        fs::create_directories(tmpExtractDir, ec);

        std::string extractOne =
            "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
            "$ErrorActionPreference='Stop'; "
            "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
            "$zip=[IO.Compression.ZipFile]::OpenRead(" + ps_quote(zipPath.string()) + "); "
            "$entry=$zip.GetEntry(" + ps_quote(rawName) + "); "
            "if(-not $entry){ throw 'entry not found' }; "
            "$tmp=" + ps_quote(tmpExtractDir) + "; "
            "$target=Join-Path $tmp $entry.Name; "
            "[IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$true); "
            "$zip.Dispose()\"";

        int oneExit = 0;
        std::string oneOut = run_capture(extractOne, oneExit);
        (void)oneOut;
        if (oneExit != 0) {
            std::cerr << "unzip error: Failed to extract " << rawName << "\n";
            fs::remove_all(tmpExtractDir, ec);
            continue;
        }

        fs::path stagedFile = fs::path(tmpExtractDir) / fs::path(rawName).filename();
        if (!fs::exists(stagedFile)) {
            std::cerr << "unzip error: Missing staged file for " << rawName << "\n";
            fs::remove_all(tmpExtractDir, ec);
            continue;
        }

        std::error_code copyEc;
        fs::copy_file(stagedFile, targetPath, fs::copy_options::overwrite_existing, copyEc);
        fs::remove_all(tmpExtractDir, ec);

        if (copyEc) {
            std::cerr << "unzip error: Failed to write " << targetPath.string() << "\n";
        }
    }

    return 0;
}

