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

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <regex>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <chrono>

namespace fs = std::filesystem;

// ============================================================================
// 1. POSIX USTAR HEADER DEFINITION & CODECS
// ============================================================================

#pragma pack(push, 1)
struct TarHeader {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
};
#pragma pack(pop)

static_assert(sizeof(TarHeader) == 512, "TarHeader size must be 512 bytes");

class OctalCodec {
public:
    static uint64_t ParseOctal(const char* str, size_t size) {
        uint64_t val = 0;
        for (size_t i = 0; i < size; ++i) {
            if (str[i] < '0' || str[i] > '7') {
                if (str[i] == '\0' || str[i] == ' ') continue;
                break;
            }
            val = (val << 3) + (str[i] - '0');
        }
        return val;
    }

    static void FormatOctal(char* dest, size_t size, uint64_t val) {
        std::snprintf(dest, size, "%0*llo", static_cast<int>(size - 1), static_cast<unsigned long long>(val));
    }

    static uint32_t CalculateChecksum(const TarHeader& hdr) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&hdr);
        uint32_t sum = 0;
        for (size_t i = 0; i < 512; ++i) {
            if (i >= 148 && i < 156) {
                sum += ' ';
            } else {
                sum += bytes[i];
            }
        }
        return sum;
    }

    static std::string GetFullTarPath(const TarHeader& hdr) {
        std::string name(hdr.name, strnlen(hdr.name, 100));
        std::string prefix(hdr.prefix, strnlen(hdr.prefix, 155));
        if (!prefix.empty()) {
            return prefix + "/" + name;
        }
        return name;
    }
};

// ============================================================================
// 2. PATH SUBSTITUTION ENGINE
// ============================================================================

struct Substitution {
    std::regex re;
    std::string replacement;
    bool global = false;
};

class SubstitutionEngine {
public:
    static bool ParseSubstitution(const std::string& arg, std::vector<Substitution>& subs) {
        if (arg.length() < 3) {
            std::cerr << "pax: invalid substitution expression '" << arg << "'\n";
            return false;
        }

        char delim = arg[0];
        size_t pos1 = 1;
        size_t pos2 = arg.find(delim, pos1);
        if (pos2 == std::string::npos) {
            std::cerr << "pax: invalid substitution expression '" << arg << "'\n";
            return false;
        }

        size_t pos3 = arg.find(delim, pos2 + 1);
        if (pos3 == std::string::npos) pos3 = arg.length();

        std::string pattern = arg.substr(pos1, pos2 - pos1);
        std::string replacement = arg.substr(pos2 + 1, pos3 - (pos2 + 1));
        std::string flags = (pos3 < arg.length()) ? arg.substr(pos3 + 1) : "";

        try {
            Substitution sub;
            sub.re = std::regex(pattern);
            sub.replacement = replacement;
            sub.global = (flags.find('g') != std::string::npos);
            subs.push_back(sub);
            return true;
        } catch (const std::regex_error& ex) {
            std::cerr << "pax: invalid regex pattern: " << ex.what() << "\n";
            return false;
        }
    }

    static std::string ApplySubstitutions(std::string path, const std::vector<Substitution>& subs) {
        for (const auto& sub : subs) {
            if (sub.global) {
                path = std::regex_replace(path, sub.re, sub.replacement);
            } else {
                path = std::regex_replace(path, sub.re, sub.replacement, std::regex_constants::format_first_only);
            }
        }
        return path;
    }
};

// ============================================================================
// 3. PAX ARCHIVER CORE
// ============================================================================

struct PaxOptions {
    bool mode_read = false;
    bool mode_write = false;
    bool verbose = false;
    bool keep = false;
    bool update = false;
    std::string archive_file = "";
    std::vector<Substitution> substitutions;
    std::vector<fs::path> positionals;
};

class PaxArchiver {
public:
    static void List(std::istream& in, bool verbose, const std::vector<Substitution>& subs) {
        TarHeader hdr;
        while (in.read(reinterpret_cast<char*>(&hdr), 512)) {
            if (hdr.name[0] == '\0') break;

            std::string full_path = OctalCodec::GetFullTarPath(hdr);
            full_path = SubstitutionEngine::ApplySubstitutions(full_path, subs);
            if (full_path.empty()) continue;

            uint64_t file_size = OctalCodec::ParseOctal(hdr.size, sizeof(hdr.size));

            if (verbose) {
                std::cout << (hdr.typeflag == '5' ? 'd' : '-') << " "
                          << std::setw(10) << file_size << " "
                          << full_path << "\n";
            } else {
                std::cout << full_path << "\n";
            }

            uint64_t blocks = (file_size + 511) / 512;
            in.seekg(blocks * 512, std::ios::cur);
        }
    }

    static void Read(std::istream& in, const fs::path& dest_dir, bool verbose, bool keep, bool update, const std::vector<Substitution>& subs) {
        TarHeader hdr;
        char buffer[512];

        while (in.read(reinterpret_cast<char*>(&hdr), 512)) {
            if (hdr.name[0] == '\0') break;

            std::string raw_path = OctalCodec::GetFullTarPath(hdr);
            std::string sub_path = SubstitutionEngine::ApplySubstitutions(raw_path, subs);
            if (sub_path.empty()) continue;

            fs::path target_path = dest_dir / sub_path;
            uint64_t file_size = OctalCodec::ParseOctal(hdr.size, sizeof(hdr.size));
            uint64_t blocks = (file_size + 511) / 512;

            if (hdr.typeflag == '5' || sub_path.back() == '/') {
                fs::create_directories(target_path);
                if (verbose) std::cout << target_path.string() << "\n";
                continue;
            }

            if (fs::exists(target_path)) {
                if (keep) {
                    in.seekg(blocks * 512, std::ios::cur);
                    continue;
                }
                if (update) {
                    uint64_t tar_mtime = OctalCodec::ParseOctal(hdr.mtime, sizeof(hdr.mtime));
                    auto local_mtime = fs::last_write_time(target_path).time_since_epoch();
                    auto local_sec = std::chrono::duration_cast<std::chrono::seconds>(local_mtime).count();
                    if (local_sec >= static_cast<int64_t>(tar_mtime)) {
                        in.seekg(blocks * 512, std::ios::cur);
                        continue;
                    }
                }
            }

            fs::create_directories(target_path.parent_path());
            std::ofstream out(target_path, std::ios::binary);

            uint64_t bytes_remaining = file_size;
            for (uint64_t b = 0; b < blocks; ++b) {
                in.read(buffer, 512);
                uint64_t to_write = (std::min)(bytes_remaining, static_cast<uint64_t>(512));
                out.write(buffer, to_write);
                bytes_remaining -= to_write;
            }
            out.close();

            if (verbose) std::cout << target_path.string() << "\n";
        }
    }

    static void WriteTarEntry(std::ostream& out, const fs::path& path, const std::string& tar_path, bool verbose) {
        TarHeader hdr = {};
        std::string clean_path = tar_path;
        std::replace(clean_path.begin(), clean_path.end(), '\\', '/');

        if (fs::is_directory(path) && clean_path.back() != '/') {
            clean_path += '/';
        }

        if (clean_path.length() > 100) {
            std::strncpy(hdr.name, clean_path.c_str(), 100);
        } else {
            std::strncpy(hdr.name, clean_path.c_str(), sizeof(hdr.name));
        }

        OctalCodec::FormatOctal(hdr.mode, sizeof(hdr.mode), fs::is_directory(path) ? 0755 : 0644);
        OctalCodec::FormatOctal(hdr.uid, sizeof(hdr.uid), 0);
        OctalCodec::FormatOctal(hdr.gid, sizeof(hdr.gid), 0);

        uint64_t file_size = fs::is_regular_file(path) ? fs::file_size(path) : 0;
        OctalCodec::FormatOctal(hdr.size, sizeof(hdr.size), file_size);

        auto ftime = fs::last_write_time(path).time_since_epoch();
        uint64_t mtime_sec = std::chrono::duration_cast<std::chrono::seconds>(ftime).count();
        OctalCodec::FormatOctal(hdr.mtime, sizeof(hdr.mtime), mtime_sec);

        hdr.typeflag = fs::is_directory(path) ? '5' : '0';
        std::strncpy(hdr.magic, "ustar", 6);
        std::strncpy(hdr.version, "00", 2);

        uint32_t chksum = OctalCodec::CalculateChecksum(hdr);
        OctalCodec::FormatOctal(hdr.chksum, sizeof(hdr.chksum), chksum);

        out.write(reinterpret_cast<const char*>(&hdr), 512);

        if (fs::is_regular_file(path) && file_size > 0) {
            std::ifstream in(path, std::ios::binary);
            char buffer[512] = { 0 };
            while (in.read(buffer, 512) || in.gcount() > 0) {
                out.write(buffer, 512);
                std::memset(buffer, 0, 512);
            }
        }

        if (verbose) std::cout << clean_path << "\n";
    }

    static void Write(std::ostream& out, const std::vector<fs::path>& sources, bool verbose, const std::vector<Substitution>& subs) {
        for (const auto& src : sources) {
            if (!fs::exists(src)) {
                std::cerr << "pax: " << src.string() << ": No such file or directory\n";
                continue;
            }

            if (fs::is_directory(src)) {
                for (const auto& entry : fs::recursive_directory_iterator(src)) {
                    std::string rel_path = entry.path().lexically_normal().string();
                    std::string sub_path = SubstitutionEngine::ApplySubstitutions(rel_path, subs);
                    if (!sub_path.empty()) {
                        WriteTarEntry(out, entry.path(), sub_path, verbose);
                    }
                }
            } else {
                std::string rel_path = src.string();
                std::string sub_path = SubstitutionEngine::ApplySubstitutions(rel_path, subs);
                if (!sub_path.empty()) {
                    WriteTarEntry(out, src, sub_path, verbose);
                }
            }
        }

        char zero_block[1024] = { 0 };
        out.write(zero_block, 1024);
    }

    static void Copy(const std::vector<fs::path>& sources, const fs::path& dest_dir, bool verbose, bool keep, bool update, const std::vector<Substitution>& subs) {
        fs::create_directories(dest_dir);

        for (const auto& src : sources) {
            if (!fs::exists(src)) {
                std::cerr << "pax: " << src.string() << ": No such file or directory\n";
                continue;
            }

            auto process_item = [&](const fs::path& item_path, const fs::path& rel_base) {
                std::string rel_str = fs::relative(item_path, rel_base).string();
                std::string sub_str = SubstitutionEngine::ApplySubstitutions(rel_str, subs);
                if (sub_str.empty()) return;

                fs::path target_path = dest_dir / sub_str;

                if (fs::is_directory(item_path)) {
                    fs::create_directories(target_path);
                    if (verbose) std::cout << target_path.string() << "\n";
                } else if (fs::is_regular_file(item_path)) {
                    if (fs::exists(target_path)) {
                        if (keep) return;
                        if (update && fs::last_write_time(target_path) >= fs::last_write_time(item_path)) return;
                    }
                    fs::create_directories(target_path.parent_path());
                    fs::copy_file(item_path, target_path, fs::copy_options::overwrite_existing);
                    if (verbose) std::cout << target_path.string() << "\n";
                }
            };

            if (fs::is_directory(src)) {
                fs::path parent_base = src.parent_path();
                if (parent_base.empty()) parent_base = ".";
                for (const auto& entry : fs::recursive_directory_iterator(src)) {
                    process_item(entry.path(), parent_base);
                }
            } else {
                fs::path target_filename = SubstitutionEngine::ApplySubstitutions(src.filename().string(), subs);
                if (!target_filename.empty()) {
                    fs::path target_path = dest_dir / target_filename;
                    fs::copy_file(src, target_path, fs::copy_options::overwrite_existing);
                    if (verbose) std::cout << target_path.string() << "\n";
                }
            }
        }
    }
};

// ============================================================================
// 4. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

class OptionParser {
public:
    static void PrintHelp() {
        std::cout << R"(pax(1)                  CrossShell for UNIX Reference Manual                  pax(1)

    NAME
        pax - portable archive interchange

    SYNOPSIS
        pax [-v] [-f ARCHIVE] [-s REPL] [PATTERN...]
        pax -r [-k] [-u] [-v] [-f ARCHIVE] [-s REPL] [PATTERN...]
        pax -w [-v] [-f ARCHIVE] [-s REPL] FILE...
        pax -r -w [-k] [-u] [-v] [-s REPL] FILE... DIRECTORY

    DESCRIPTION
        pax reads, writes, and lists the members of an archive file and copies
        directory hierarchies. A variety of archive formats are supported,
        defaulting to POSIX ustar.

    OPTIONS
        -r, --read
            Read an archive from standard input or -f file and extract files.

        -w, --write
            Write files to standard output or -f file in archive format.

        -f, --file ARCHIVE
            Specify archive file name instead of standard input/output.

        -k, --keep
            Prevent overwriting existing files when extracting or copying.

        -u, --update
            Copy or extract only if the source file is newer than target.

        -v, --verbose
            List file names and metadata as they are processed.

        -s, --substitute REPL
            Modify file names using regular expression substitution /old/new/[g].

        -h, --help
            Display this reference manual.

        -V, --version
            Display version information.

    EXAMPLES
        pax -f archive.tar
            List contents of tar archive.

        pax -r -f archive.tar .
            Extract archive into current directory.

        pax -w -f backup.tar src/
            Create new archive backup.tar containing src/ directory.

        pax -r -w -v srcdir destdir
            Copy directory hierarchy from srcdir to destdir preserving structure.

    CrossShell for UNIX                                                    pax(1)
)";
    }

    static void PrintVersion() {
        std::cout << "pax v1.0.0\n";
    }

    bool Parse(int argc, char* argv[], PaxOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--") {
                for (int j = i + 1; j < argc; ++j) {
                    opts.positionals.push_back(argv[j]);
                }
                break;
            } else if (arg == "--help" || arg == "-h") {
                PrintHelp();
                exitEarly = true;
                return true;
            } else if (arg == "--version" || arg == "-V") {
                PrintVersion();
                exitEarly = true;
                return true;
            } else if (arg == "-r" || arg == "--read") {
                opts.mode_read = true;
            } else if (arg == "-w" || arg == "--write") {
                opts.mode_write = true;
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg == "-k" || arg == "--keep") {
                opts.keep = true;
            } else if (arg == "-u" || arg == "--update") {
                opts.update = true;
            } else if (arg == "-f" || arg == "--file") {
                if (i + 1 >= argc) {
                    std::cerr << "pax: option '" << arg << "' requires a filename\n";
                    return false;
                }
                opts.archive_file = argv[++i];
            } else if (arg == "-s" || arg == "--substitute") {
                if (i + 1 >= argc) {
                    std::cerr << "pax: option '" << arg << "' requires an expression\n";
                    return false;
                }
                if (!SubstitutionEngine::ParseSubstitution(argv[++i], opts.substitutions)) {
                    return false;
                }
            } else if (arg.rfind("-", 0) == 0 && arg.length() > 1) {
                for (size_t j = 1; j < arg.length(); ++j) {
                    char c = arg[j];
                    if (c == 'r') opts.mode_read = true;
                    else if (c == 'w') opts.mode_write = true;
                    else if (c == 'v') opts.verbose = true;
                    else if (c == 'k') opts.keep = true;
                    else if (c == 'u') opts.update = true;
                    else {
                        std::cerr << "pax: invalid option '-" << c << "'\n";
                        return false;
                    }
                }
            } else {
                opts.positionals.push_back(arg);
            }
        }
        return true;
    }
};

class PaxApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]) {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        PaxOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        // MODE 4: COPY MODE (-r -w)
        if (opts.mode_read && opts.mode_write) {
            if (opts.positionals.size() < 2) {
                std::cerr << "pax: copy mode requires source files and a target directory\n";
                return 1;
            }
            fs::path dest_dir = opts.positionals.back();
            std::vector<fs::path> sources(opts.positionals.begin(), opts.positionals.end() - 1);
            PaxArchiver::Copy(sources, dest_dir, opts.verbose, opts.keep, opts.update, opts.substitutions);
            return 0;
        }

        // MODE 2: READ / EXTRACT MODE (-r)
        if (opts.mode_read && !opts.mode_write) {
            fs::path dest_dir = opts.positionals.empty() ? "." : opts.positionals[0];
            if (!opts.archive_file.empty()) {
                std::ifstream in(opts.archive_file, std::ios::binary);
                if (!in) {
                    std::cerr << "pax: cannot open archive " << opts.archive_file << "\n";
                    return 1;
                }
                PaxArchiver::Read(in, dest_dir, opts.verbose, opts.keep, opts.update, opts.substitutions);
            } else {
                PaxArchiver::Read(std::cin, dest_dir, opts.verbose, opts.keep, opts.update, opts.substitutions);
            }
            return 0;
        }

        // MODE 3: WRITE / ARCHIVE MODE (-w)
        if (!opts.mode_read && opts.mode_write) {
            if (opts.positionals.empty()) {
                std::cerr << "pax: write mode requires file/directory arguments\n";
                return 1;
            }
            if (!opts.archive_file.empty()) {
                std::ofstream out(opts.archive_file, std::ios::binary);
                if (!out) {
                    std::cerr << "pax: cannot create archive " << opts.archive_file << "\n";
                    return 1;
                }
                PaxArchiver::Write(out, opts.positionals, opts.verbose, opts.substitutions);
            } else {
                PaxArchiver::Write(std::cout, opts.positionals, opts.verbose, opts.substitutions);
            }
            return 0;
        }

        // MODE 1: LIST MODE (default)
        if (!opts.archive_file.empty()) {
            std::ifstream in(opts.archive_file, std::ios::binary);
            if (!in) {
                std::cerr << "pax: cannot open archive " << opts.archive_file << "\n";
                return 1;
            }
            PaxArchiver::List(in, opts.verbose, opts.substitutions);
        } else {
            PaxArchiver::List(std::cin, opts.verbose, opts.substitutions);
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    PaxApplication app;
    return app.Run(argc, argv);
}
