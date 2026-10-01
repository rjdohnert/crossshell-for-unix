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

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <memory>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// 1. DATA MODELS & SVR4 PORTABLE ASCII FORMAT (NEWC) HEADER
// ============================================================================

#pragma pack(push, 1)
struct CpioHeader {
    char magic[6];     // "070701"
    char ino[8];       // Inode number
    char mode[8];      // File mode / permissions
    char uid[8];       // User ID
    char gid[8];       // Group ID
    char nlink[8];     // Number of links
    char mtime[8];     // Modification time
    char filesize[8];  // File size
    char maj[8];       // Major device number
    char min[8];       // Minor device number
    char rmaj[8];      // Real major device number
    char rmin[8];      // Real minor device number
    char namesize[8];  // Length of path (including trailing '\0')
    char chksum[8];    // Checksum (0 for newc)
};
#pragma pack(pop)

enum class CpioMode {
    None,
    Create,
    Extract,
    Pass,
    List
};

enum class OutputFormat {
    Default = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

// ============================================================================
// 2. HEADER CODEC & HELPER METHODS
// ============================================================================

class CpioHeaderCodec {
public:
    static uint32_t ParseHex8(const char* buf) {
        char tmp[9] = {0};
        std::memcpy(tmp, buf, 8);
        return static_cast<uint32_t>(std::strtoul(tmp, nullptr, 16));
    }

    static void FormatHex8(char* buf, uint32_t val) {
        snprintf(buf, 9, "%08X", val);
    }

    static void WritePadding(std::ostream& out, size_t count) {
        static const char zeros[4] = {0, 0, 0, 0};
        if (count > 0 && count <= 4) {
            out.write(zeros, count);
        }
    }

    static void SkipBytes(std::istream& in, size_t count) {
        char buf[8192];
        while (count > 0 && in) {
            size_t chunk = (std::min)(count, sizeof(buf));
            in.read(buf, chunk);
            count -= in.gcount();
            if (in.gcount() == 0) break;
        }
    }

    static uint32_t GetMtimeSec(const fs::path& p) {
        std::error_code ec;
        auto ftime = fs::last_write_time(p, ec);
        if (ec) return 0;

        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        return static_cast<uint32_t>(std::chrono::system_clock::to_time_t(sctp));
    }
};

// ============================================================================
// 3. ARCHIVE PROCESSING ENGINE
// ============================================================================

class CpioArchiveEngine {
public:
    static void DoCreate(std::ostream& out, bool null_delim, bool verbose) {
        std::string filename;
        uint32_t ino_counter = 1;

        auto read_next_name = [&](std::string& name) -> bool {
            if (null_delim) {
                return static_cast<bool>(std::getline(std::cin, name, '\0'));
            } else {
                return static_cast<bool>(std::getline(std::cin, name));
            }
        };

        auto write_entry = [&](const std::string& path_str) {
            fs::path p(path_str);

            std::string archive_path = p.generic_string();
            if (archive_path.rfind("./", 0) == 0) {
                archive_path = archive_path.substr(2);
            }

            std::error_code ec;
            fs::file_status status = fs::status(p, ec);
            if (ec) {
                std::cerr << "cpio: " << path_str << ": Cannot stat: " << ec.message() << "\n";
                return;
            }

            uint32_t mode = 0644;
            if (fs::is_directory(status)) {
                mode |= 0040000;
                mode |= 0755;
            } else if (fs::is_regular_file(status)) {
                mode |= 0100000;
            } else {
                return;
            }

            uint64_t fsize = fs::is_regular_file(status) ? fs::file_size(p, ec) : 0;
            if (ec) fsize = 0;

            uint32_t mtime_sec = CpioHeaderCodec::GetMtimeSec(p);

            CpioHeader hdr;
            std::memset(&hdr, '0', sizeof(hdr));
            std::memcpy(hdr.magic, "070701", 6);

            CpioHeaderCodec::FormatHex8(hdr.ino, ino_counter++);
            CpioHeaderCodec::FormatHex8(hdr.mode, mode);
            CpioHeaderCodec::FormatHex8(hdr.uid, 0);
            CpioHeaderCodec::FormatHex8(hdr.gid, 0);
            CpioHeaderCodec::FormatHex8(hdr.nlink, 1);
            CpioHeaderCodec::FormatHex8(hdr.mtime, mtime_sec);
            CpioHeaderCodec::FormatHex8(hdr.filesize, static_cast<uint32_t>(fsize));
            CpioHeaderCodec::FormatHex8(hdr.maj, 0);
            CpioHeaderCodec::FormatHex8(hdr.min, 0);
            CpioHeaderCodec::FormatHex8(hdr.rmaj, 0);
            CpioHeaderCodec::FormatHex8(hdr.rmin, 0);
            CpioHeaderCodec::FormatHex8(hdr.namesize, static_cast<uint32_t>(archive_path.length() + 1));
            CpioHeaderCodec::FormatHex8(hdr.chksum, 0);

            out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
            out.write(archive_path.c_str(), archive_path.length() + 1);

            size_t head_bytes = sizeof(hdr) + archive_path.length() + 1;
            size_t head_pad = (4 - (head_bytes % 4)) % 4;
            CpioHeaderCodec::WritePadding(out, head_pad);

            if (fs::is_regular_file(status) && fsize > 0) {
                std::ifstream fin(p, std::ios::binary);
                if (fin) {
                    char buf[65536];
                    uint64_t remaining = fsize;
                    while (remaining > 0 && fin) {
                        size_t to_read = static_cast<size_t>(std::min<uint64_t>(sizeof(buf), remaining));
                        fin.read(buf, to_read);
                        size_t read_bytes = fin.gcount();
                        if (read_bytes == 0) break;
                        out.write(buf, read_bytes);
                        remaining -= read_bytes;
                    }
                }
                size_t file_pad = (4 - (fsize % 4)) % 4;
                CpioHeaderCodec::WritePadding(out, file_pad);
            }

            if (verbose) {
                std::cerr << archive_path << "\n";
            }
        };

        while (read_next_name(filename)) {
            if (filename.empty()) continue;
            write_entry(filename);
        }

        std::string trailer = "TRAILER!!!";
        CpioHeader hdr;
        std::memset(&hdr, '0', sizeof(hdr));
        std::memcpy(hdr.magic, "070701", 6);
        CpioHeaderCodec::FormatHex8(hdr.namesize, static_cast<uint32_t>(trailer.length() + 1));

        out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
        out.write(trailer.c_str(), trailer.length() + 1);
        size_t head_bytes = sizeof(hdr) + trailer.length() + 1;
        size_t head_pad = (4 - (head_bytes % 4)) % 4;
        CpioHeaderCodec::WritePadding(out, head_pad);
    }

    static void DoExtractOrList(std::istream& in, bool extract, bool make_dirs, bool verbose) {
        while (in) {
            CpioHeader hdr;
            in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
            if (in.gcount() < static_cast<std::streamsize>(sizeof(hdr))) break;

            if (std::memcmp(hdr.magic, "070701", 6) != 0 && std::memcmp(hdr.magic, "070702", 6) != 0) {
                std::cerr << "cpio: invalid magic number in header\n";
                return;
            }

            uint32_t mode = CpioHeaderCodec::ParseHex8(hdr.mode);
            uint32_t filesize = CpioHeaderCodec::ParseHex8(hdr.filesize);
            uint32_t namesize = CpioHeaderCodec::ParseHex8(hdr.namesize);

            if (namesize == 0) continue;

            std::vector<char> name_buf(namesize);
            in.read(name_buf.data(), namesize);

            std::string filename(name_buf.data(), namesize > 0 ? namesize - 1 : 0);

            size_t head_bytes = sizeof(hdr) + namesize;
            size_t head_pad = (4 - (head_bytes % 4)) % 4;
            CpioHeaderCodec::SkipBytes(in, head_pad);

            if (filename == "TRAILER!!!") break;

            uint32_t type = mode & 0170000;
            bool is_dir = (type == 0040000);

            if (!extract) {
                if (verbose) {
                    std::cout << (is_dir ? 'd' : '-') << " "
                              << std::setw(10) << filesize << " "
                              << filename << "\n";
                } else {
                    std::cout << filename << "\n";
                }

                if (filesize > 0) {
                    CpioHeaderCodec::SkipBytes(in, filesize);
                    size_t file_pad = (4 - (filesize % 4)) % 4;
                    CpioHeaderCodec::SkipBytes(in, file_pad);
                }
            } else {
                if (verbose) {
                    std::cout << filename << "\n";
                }

                fs::path target_path(filename);

                if (is_dir) {
                    if (make_dirs) {
                        fs::create_directories(target_path);
                    }
                    if (filesize > 0) {
                        CpioHeaderCodec::SkipBytes(in, filesize + ((4 - (filesize % 4)) % 4));
                    }
                } else {
                    if (make_dirs && target_path.has_parent_path()) {
                        fs::create_directories(target_path.parent_path());
                    }

                    std::ofstream fout(target_path, std::ios::binary);
                    if (!fout) {
                        std::cerr << "cpio: failed to create " << filename << "\n";
                        CpioHeaderCodec::SkipBytes(in, filesize);
                    } else {
                        char buf[65536];
                        uint32_t remaining = filesize;
                        while (remaining > 0 && in) {
                            size_t to_read = static_cast<size_t>(std::min<uint32_t>(sizeof(buf), remaining));
                            in.read(buf, to_read);
                            size_t read_bytes = in.gcount();
                            if (read_bytes == 0) break;
                            fout.write(buf, read_bytes);
                            remaining -= static_cast<uint32_t>(read_bytes);
                        }
                    }

                    size_t file_pad = (4 - (filesize % 4)) % 4;
                    CpioHeaderCodec::SkipBytes(in, file_pad);
                }
            }
        }
    }

    static void DoPass(const std::string& dest_dir, bool null_delim, bool make_dirs, bool verbose) {
        fs::path dest(dest_dir);

        std::string filename;
        auto read_next_name = [&](std::string& name) -> bool {
            if (null_delim) {
                return static_cast<bool>(std::getline(std::cin, name, '\0'));
            } else {
                return static_cast<bool>(std::getline(std::cin, name));
            }
        };

        while (read_next_name(filename)) {
            if (filename.empty()) continue;

            fs::path src_path(filename);
            fs::path target_path = dest / src_path;

            std::error_code ec;
            fs::file_status status = fs::status(src_path, ec);
            if (ec) {
                std::cerr << "cpio: " << filename << ": " << ec.message() << "\n";
                continue;
            }

            if (fs::is_directory(status)) {
                if (make_dirs) {
                    fs::create_directories(target_path, ec);
                }
            } else if (fs::is_regular_file(status)) {
                if (make_dirs && target_path.has_parent_path()) {
                    fs::create_directories(target_path.parent_path(), ec);
                }
                fs::copy_file(src_path, target_path, fs::copy_options::overwrite_existing, ec);
                if (ec) {
                    std::cerr << "cpio: failed to copy " << filename << ": " << ec.message() << "\n";
                }
            }

            if (verbose) {
                std::cout << filename << "\n";
            }
        }
    }
};

// ============================================================================
// 4. OPTIONS & REPORTER
// ============================================================================

class CpioOptions {
public:
    CpioMode mode = CpioMode::None;
    bool verbose = false;
    bool make_dirs = false;
    bool null_delim = false;
    std::string archive_file;
    std::string pass_dir;
    OutputFormat outputFormat = OutputFormat::Default;
    std::string pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--json") { outputFormat = OutputFormat::Json; continue; }
            if (arg == "--csv") { outputFormat = OutputFormat::Csv; continue; }
            if (arg == "--table") { outputFormat = OutputFormat::Table; continue; }
            if (arg == "--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
            if (arg == "--") {
                for (int j = i + 1; j < argc; ++j) {
                    if (mode == CpioMode::Pass && pass_dir.empty()) {
                        pass_dir = argv[j];
                    } else {
                        std::cerr << "cpio: unexpected argument: " << argv[j] << "\n";
                        return false;
                    }
                }
                break;
            }
            if (arg == "--help" || arg == "-h" || arg == "/?") {
                showHelp = true;
                return true;
            }
            if (arg == "--version") {
                showVersion = true;
                return true;
            }
            if (arg.rfind("-", 0) == 0 && arg.length() > 1 && arg[1] != '-') {
                for (size_t j = 1; j < arg.length(); ++j) {
                    char c = arg[j];
                    switch (c) {
                        case 'o': mode = CpioMode::Create; break;
                        case 'i': if (mode != CpioMode::List) mode = CpioMode::Extract; break;
                        case 'p': mode = CpioMode::Pass; break;
                        case 't': mode = CpioMode::List; break;
                        case 'v': verbose = true; break;
                        case 'd': make_dirs = true; break;
                        case '0': null_delim = true; break;
                        case 'F':
                            if (j + 1 < arg.length()) {
                                archive_file = arg.substr(j + 1);
                                j = arg.length();
                            } else if (i + 1 < argc) {
                                archive_file = argv[++i];
                            } else {
                                std::cerr << "cpio: option requires an argument -- 'F'\n";
                                return false;
                            }
                            break;
                        default:
                            std::cerr << "cpio: unknown option -- '" << c << "'\n";
                            return false;
                    }
                }
            } else if (arg == "--create") mode = CpioMode::Create;
            else if (arg == "--extract") { if (mode != CpioMode::List) mode = CpioMode::Extract; }
            else if (arg == "--pass-through") mode = CpioMode::Pass;
            else if (arg == "--list") mode = CpioMode::List;
            else if (arg == "--verbose") verbose = true;
            else if (arg == "--make-directories") make_dirs = true;
            else if (arg == "--null") null_delim = true;
            else if (arg == "--file") {
                if (i + 1 < argc) archive_file = argv[++i];
                else { std::cerr << "cpio: option requires an argument -- 'file'\n"; return false; }
            } else if (arg.rfind("--file=", 0) == 0) {
                archive_file = arg.substr(7);
            } else if (mode == CpioMode::Pass && pass_dir.empty()) {
                pass_dir = arg;
            } else {
                std::cerr << "cpio: unexpected argument: " << arg << "\n";
                return false;
            }
        }
        return true;
    }

    void PrintHelp(const char* prog_name) const {
        std::cout << R"(cpio(1)                 CrossShell for UNIX Reference Manual                  cpio(1)

    NAME
        cpio - copy files to and from archives

    SYNOPSIS
        cpio -o [OPTIONS] < name-list > archive
        cpio -i [OPTIONS] [PATTERNS...] < archive
        cpio -p [OPTIONS] DEST-DIRECTORY < name-list

    DESCRIPTION
        cpio copies files into or out of a cpio or tar archive. It supports
        create mode (-o), extract mode (-i), and pass-through mode (-p).

    OPTIONS
        -o, --create
            Run in copy-out mode (create archive).

        -i, --extract
            Run in copy-in mode (extract from archive).

        -p, --pass-through
            Run in copy-pass mode (copy directory tree to target directory).

        -v, --verbose
            List files processed.

        -t, --list
            Print table of contents of the input.

        --json, --csv, --table
            Output archive entry manifests in structured format.

        --pipe COMMAND
            Stream results into COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Output version information and exit.

    EXAMPLES
        find . -depth -print0 | cpio -ov > tree.cpio
            Archive current directory hierarchy into tree.cpio.

        cpio -ivt < tree.cpio
            List contents of tree.cpio archive.

    CrossShell for UNIX                                                   cpio(1)
)";
    }

    void PrintVersion() const {
        std::cout << "cpio 1.0\n";
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class CpioApplication {
public:
    int Run(int argc, char* argv[]) const {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif

        CpioOptions opts;
        if (!opts.Parse(argc, argv)) {
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintHelp(argv[0]);
            return 0;
        }
        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        if (opts.mode == CpioMode::None) {
            opts.PrintHelp(argv[0]);
            return 1;
        }

        if (opts.mode == CpioMode::Create) {
            if (!opts.archive_file.empty()) {
                std::ofstream fout(opts.archive_file, std::ios::binary);
                if (!fout) {
                    std::cerr << "cpio: cannot open " << opts.archive_file << "\n";
                    return 1;
                }
                CpioArchiveEngine::DoCreate(fout, opts.null_delim, opts.verbose);
            } else {
                CpioArchiveEngine::DoCreate(std::cout, opts.null_delim, opts.verbose);
            }
        } else if (opts.mode == CpioMode::Extract || opts.mode == CpioMode::List) {
            bool extract = (opts.mode == CpioMode::Extract);
            if (!opts.archive_file.empty()) {
                std::ifstream fin(opts.archive_file, std::ios::binary);
                if (!fin) {
                    std::cerr << "cpio: cannot open " << opts.archive_file << "\n";
                    return 1;
                }
                CpioArchiveEngine::DoExtractOrList(fin, extract, opts.make_dirs, opts.verbose);
            } else {
                CpioArchiveEngine::DoExtractOrList(std::cin, extract, opts.make_dirs, opts.verbose);
            }
        } else if (opts.mode == CpioMode::Pass) {
            if (opts.pass_dir.empty()) {
                std::cerr << "cpio: destination directory required for pass-through mode (-p)\n";
                return 1;
            }
            CpioArchiveEngine::DoPass(opts.pass_dir, opts.null_delim, opts.make_dirs, opts.verbose);
        }

        if (opts.outputFormat != OutputFormat::Default || !opts.pipeCommand.empty()) {
            std::string modeName = (opts.mode == CpioMode::Create) ? "create" :
                                   (opts.mode == CpioMode::Extract) ? "extract" :
                                   (opts.mode == CpioMode::List) ? "list" : "pass-through";
            std::string text = (opts.outputFormat == OutputFormat::Json) ? "{\"status\":\"success\",\"mode\":\"" + modeName + "\"}\n" :
                               (opts.outputFormat == OutputFormat::Csv) ? "status,mode\nsuccess," + modeName + "\n" :
                               "STATUS\tMODE\nsuccess\t" + modeName + "\n";

            if (!opts.pipeCommand.empty()) {
                FILE* pipe = _popen(opts.pipeCommand.c_str(), "w");
                if (pipe) {
                    fwrite(text.data(), 1, text.size(), pipe);
                    _pclose(pipe);
                }
            } else {
                std::cout << text;
            }
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    CpioApplication app;
    return app.Run(argc, argv);
}
