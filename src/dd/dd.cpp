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

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. RAII HANDLE & SIZE PARSING UTILITIES
// ============================================================================
class ScopedHandle {
private:
    HANDLE m_handle = INVALID_HANDLE_VALUE;
    bool m_is_standard = false;

public:
    ScopedHandle() = default;

    ScopedHandle(HANDLE h, bool is_standard = false)
        : m_handle(h), m_is_standard(is_standard) {}

    ~ScopedHandle() {
        Close();
    }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept
        : m_handle(other.m_handle), m_is_standard(other.m_is_standard) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            m_is_standard = other.m_is_standard;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    void Reset(HANDLE h, bool is_standard = false) {
        Close();
        m_handle = h;
        m_is_standard = is_standard;
    }

    void Close() {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr && !m_is_standard) {
            CloseHandle(m_handle);
        }
        m_handle = INVALID_HANDLE_VALUE;
    }

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr; }
};

class SizeParser {
public:
    static uint64_t ParseSize(const std::string& str) {
        size_t x_pos = str.find('x');
        if (x_pos != std::string::npos) {
            uint64_t a = ParseSize(str.substr(0, x_pos));
            uint64_t b = ParseSize(str.substr(x_pos + 1));
            return a * b;
        }

        uint64_t multiplier = 1;
        std::string num_part = str;

        if (!str.empty()) {
            char suffix = str.back();
            if (suffix == 'b' || suffix == 'B') {
                multiplier = 512;
                num_part = str.substr(0, str.size() - 1);
            } else if (suffix == 'k' || suffix == 'K') {
                multiplier = 1024;
                num_part = str.substr(0, str.size() - 1);
            } else if (suffix == 'M' || suffix == 'm') {
                multiplier = 1024ULL * 1024ULL;
                num_part = str.substr(0, str.size() - 1);
            } else if (suffix == 'G' || suffix == 'g') {
                multiplier = 1024ULL * 1024ULL * 1024ULL;
                num_part = str.substr(0, str.size() - 1);
            }
        }

        try {
            return std::stoull(num_part) * multiplier;
        } catch (...) {
            return 0;
        }
    }
};

// ============================================================================
// 2. CONFIGURATION & STATS MODELS
// ============================================================================
enum class StatusLevel {
    DEFAULT,
    NONE,
    NOXFER,
    PROGRESS
};

struct DdOptions {
    std::string if_path;
    std::string of_path;
    uint64_t ibs = 512;
    uint64_t obs = 512;
    uint64_t count = UINT64_MAX;
    uint64_t skip = 0;
    uint64_t seek = 0;
    StatusLevel status = StatusLevel::DEFAULT;

    bool conv_ucase = false;
    bool conv_lcase = false;
    bool conv_swab = false;
    bool conv_sync = false;
    bool conv_noerror = false;
    bool conv_notrunc = false;
    bool conv_excl = false;
    int output_format = 0;
    std::string pipe_command;
};

struct TransferStats {
    uint64_t rec_in_f = 0;
    uint64_t rec_in_p = 0;
    uint64_t rec_out_f = 0;
    uint64_t rec_out_p = 0;
    uint64_t total_bytes = 0;
    std::chrono::high_resolution_clock::time_point start_time;
};

// ============================================================================
// 3. DATA TRANSFORMER & PROGRESS REPORTER
// ============================================================================
class DataTransformer {
public:
    static void Transform(std::vector<char>& buffer, DWORD& bytes_read, const DdOptions& opt) {
        // conv=sync padding
        if (opt.conv_sync && bytes_read < opt.ibs) {
            std::fill(buffer.begin() + bytes_read, buffer.begin() + opt.ibs, 0);
            bytes_read = static_cast<DWORD>(opt.ibs);
        }

        // conv=ucase / conv=lcase
        if (opt.conv_ucase) {
            for (DWORD i = 0; i < bytes_read; ++i) {
                buffer[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(buffer[i])));
            }
        } else if (opt.conv_lcase) {
            for (DWORD i = 0; i < bytes_read; ++i) {
                buffer[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(buffer[i])));
            }
        }

        // conv=swab
        if (opt.conv_swab) {
            for (DWORD i = 0; i + 1 < bytes_read; i += 2) {
                std::swap(buffer[i], buffer[i + 1]);
            }
        }
    }
};

class ProgressReporter {
public:
    static void PrintStats(const TransferStats& stats, double elapsed_seconds, StatusLevel status, bool is_progress) {
        if (status == StatusLevel::NONE) return;

        if (is_progress) {
            double rate = (elapsed_seconds > 0) ? (stats.total_bytes / elapsed_seconds) : 0;
            std::string unit = "B/s";
            double rate_disp = rate;
            if (rate_disp >= 1024 * 1024 * 1024) { rate_disp /= (1024 * 1024 * 1024); unit = "GB/s"; }
            else if (rate_disp >= 1024 * 1024)   { rate_disp /= (1024 * 1024); unit = "MB/s"; }
            else if (rate_disp >= 1024)          { rate_disp /= 1024; unit = "kB/s"; }

            std::cerr << "\r" << stats.total_bytes << " bytes (" << std::fixed << std::setprecision(1)
                      << rate_disp << " " << unit << ") copied, " << std::setprecision(1) << elapsed_seconds << " s" << std::flush;
            return;
        }

        std::cerr << stats.rec_in_f << "+" << stats.rec_in_p << " records in\n";
        std::cerr << stats.rec_out_f << "+" << stats.rec_out_p << " records out\n";

        if (status != StatusLevel::NOXFER) {
            double rate = (elapsed_seconds > 0) ? (stats.total_bytes / elapsed_seconds) : 0;
            std::string unit = "B/s";
            double rate_disp = rate;
            if (rate_disp >= 1024 * 1024 * 1024) { rate_disp /= (1024 * 1024 * 1024); unit = "GB/s"; }
            else if (rate_disp >= 1024 * 1024)   { rate_disp /= (1024 * 1024); unit = "MB/s"; }
            else if (rate_disp >= 1024)          { rate_disp /= 1024; unit = "kB/s"; }

            std::cerr << stats.total_bytes << " bytes copied, "
                      << std::fixed << std::setprecision(3) << elapsed_seconds << " s, "
                      << std::setprecision(1) << rate_disp << " " << unit << "\n";
        }
    }
};

// ============================================================================
// 4. BLOCK COPIER ENGINE
// ============================================================================
class BlockCopier {
public:
    static bool Execute(const DdOptions& opt, TransferStats& stats) {
        stats = TransferStats();
        stats.start_time = std::chrono::high_resolution_clock::now();

        ScopedHandle hIn;
        if (opt.if_path.empty()) {
            hIn.Reset(GetStdHandle(STD_INPUT_HANDLE), true);
            _setmode(_fileno(stdin), _O_BINARY);
        } else {
            HANDLE h = CreateFileA(opt.if_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h == INVALID_HANDLE_VALUE) {
                std::cerr << "dd: failed to open '" << opt.if_path << "': Error " << GetLastError() << "\n";
                return false;
            }
            hIn.Reset(h, false);
        }

        ScopedHandle hOut;
        if (opt.of_path.empty()) {
            hOut.Reset(GetStdHandle(STD_OUTPUT_HANDLE), true);
            _setmode(_fileno(stdout), _O_BINARY);
        } else {
            DWORD creation = opt.conv_excl ? CREATE_NEW : (opt.conv_notrunc ? OPEN_ALWAYS : CREATE_ALWAYS);
            HANDLE h = CreateFileA(opt.of_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                   creation, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h == INVALID_HANDLE_VALUE) {
                std::cerr << "dd: failed to open '" << opt.of_path << "': Error " << GetLastError() << "\n";
                return false;
            }
            hOut.Reset(h, false);
        }

        // Handle skip=
        if (opt.skip > 0) {
            LARGE_INTEGER offset;
            offset.QuadPart = static_cast<LONGLONG>(opt.skip * opt.ibs);
            if (!SetFilePointerEx(hIn.Get(), offset, NULL, FILE_CURRENT)) {
                // Pipe fallback: read and discard
                std::vector<char> skip_buf(opt.ibs);
                for (uint64_t i = 0; i < opt.skip; ++i) {
                    DWORD bytes_read = 0;
                    if (!ReadFile(hIn.Get(), skip_buf.data(), static_cast<DWORD>(opt.ibs), &bytes_read, NULL) || bytes_read == 0) {
                        break;
                    }
                }
            }
        }

        // Handle seek=
        if (opt.seek > 0) {
            LARGE_INTEGER offset;
            offset.QuadPart = static_cast<LONGLONG>(opt.seek * opt.obs);
            SetFilePointerEx(hOut.Get(), offset, NULL, FILE_CURRENT);
        }

        std::vector<char> in_buf(opt.ibs);
        auto last_progress_time = stats.start_time;
        uint64_t blocks_processed = 0;

        while (blocks_processed < opt.count) {
            DWORD bytes_to_read = static_cast<DWORD>(opt.ibs);
            DWORD bytes_read = 0;

            BOOL read_success = ReadFile(hIn.Get(), in_buf.data(), bytes_to_read, &bytes_read, NULL);
            if (!read_success) {
                if (opt.conv_noerror) {
                    std::cerr << "dd: error reading input file: Error " << GetLastError() << "\n";
                    std::fill(in_buf.begin(), in_buf.end(), 0);
                    bytes_read = static_cast<DWORD>(opt.ibs);
                } else {
                    std::cerr << "dd: error reading input file: Error " << GetLastError() << "\n";
                    break;
                }
            }

            if (bytes_read == 0) break; // EOF

            if (bytes_read == opt.ibs) stats.rec_in_f++;
            else stats.rec_in_p++;

            blocks_processed++;

            DataTransformer::Transform(in_buf, bytes_read, opt);

            DWORD bytes_written = 0;
            BOOL write_success = WriteFile(hOut.Get(), in_buf.data(), bytes_read, &bytes_written, NULL);
            if (!write_success || bytes_written < bytes_read) {
                std::cerr << "dd: error writing output file: Error " << GetLastError() << "\n";
                if (bytes_written > 0) {
                    stats.total_bytes += bytes_written;
                    if (bytes_written == opt.obs) stats.rec_out_f++; else stats.rec_out_p++;
                }
                break;
            }

            stats.total_bytes += bytes_written;
            if (bytes_written == opt.obs) stats.rec_out_f++;
            else stats.rec_out_p++;

            if (opt.status == StatusLevel::PROGRESS) {
                auto now = std::chrono::high_resolution_clock::now();
                double elapsed_since_last = std::chrono::duration<double>(now - last_progress_time).count();
                if (elapsed_since_last >= 0.5) {
                    double elapsed_total = std::chrono::duration<double>(now - stats.start_time).count();
                    ProgressReporter::PrintStats(stats, elapsed_total, opt.status, true);
                    last_progress_time = now;
                }
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        double total_elapsed = std::chrono::duration<double>(end_time - stats.start_time).count();

        if (opt.status == StatusLevel::PROGRESS) {
            std::cerr << "\n";
        }

        ProgressReporter::PrintStats(stats, total_elapsed, opt.status, false);
        return true;
    }
};

// ============================================================================
// 5. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================
class OptionParser {
public:
    static void DisplayHelp(const char* prog_name = nullptr) {
        (void)prog_name;
        std::cout << R"(dd(1)                   CrossShell for UNIX Reference Manual                  dd(1)

    NAME
        dd - convert and copy a file

    SYNOPSIS
        dd [OPERANDS...]
        dd [OPTIONS]

    DESCRIPTION
        Copy a file, converting and formatting according to the operands.

    OPERANDS
        if=FILE
            Read from FILE instead of standard input.

        of=FILE
            Write to FILE instead of standard output.

        bs=BYTES
            Read and write up to BYTES bytes at a time (default: 512).

        ibs=BYTES, obs=BYTES
            Read or write up to BYTES bytes at a time.

        count=N
            Copy only N input blocks.

        skip=N
            Skip N ibs-sized blocks at start of input.

        seek=N
            Skip N obs-sized blocks at start of output.

        status=LEVEL
            Transfer diagnostic level: 'none', 'noxfer', or 'progress'.

        conv=CONVS
            Comma-separated conversion list: ucase, lcase, swab, sync,
            noerror, notrunc, excl.

    OPTIONS
        --json, --csv, --table
            Emit transfer status records in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send status output directly through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        dd if=input.img of=output.img bs=4k count=100
            Copy 100 4KB blocks from input.img to output.img.

        dd if=source.dat of=dest.dat conv=ucase
            Convert source text to uppercase while copying.

        dd if=raw.bin of=backup.bin bs=1M status=progress
            Copy with progress indicator.

    CrossShell for UNIX                                                    dd(1)
)";
    }

    static void DisplayVersion() {
        std::cout << "dd (Extended Utilities) 2.0\n"
                  << "Copyright (C) 2026 Roberto J. Dohnert\n";
    }

    bool Parse(int argc, char* argv[], DdOptions& opt, bool& exitEarly) const {
        exitEarly = false;
        bool end_of_opts = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (!end_of_opts) {
                if (arg == "--help" || arg == "-h") {
                    DisplayHelp(argv[0]);
                    exitEarly = true;
                    return true;
                }
                if (arg == "--version") {
                    DisplayVersion();
                    exitEarly = true;
                    return true;
                }
                if (arg == "--json") { opt.output_format = 1; continue; }
                if (arg == "--csv") { opt.output_format = 2; continue; }
                if (arg == "--table") { opt.output_format = 3; continue; }
                if (arg == "--pipe" && i + 1 < argc) { opt.pipe_command = argv[++i]; continue; }
                if (arg == "--") { end_of_opts = true; continue; }
            }

            size_t eq_pos = arg.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = arg.substr(0, eq_pos);
                std::string val = arg.substr(eq_pos + 1);

                if (key == "if") opt.if_path = val;
                else if (key == "of") opt.of_path = val;
                else if (key == "bs") {
                    uint64_t sz = SizeParser::ParseSize(val);
                    if (sz > 0) { opt.ibs = sz; opt.obs = sz; }
                }
                else if (key == "ibs") {
                    uint64_t sz = SizeParser::ParseSize(val);
                    if (sz > 0) opt.ibs = sz;
                }
                else if (key == "obs") {
                    uint64_t sz = SizeParser::ParseSize(val);
                    if (sz > 0) opt.obs = sz;
                }
                else if (key == "count") opt.count = SizeParser::ParseSize(val);
                else if (key == "skip") opt.skip = SizeParser::ParseSize(val);
                else if (key == "seek") opt.seek = SizeParser::ParseSize(val);
                else if (key == "status") {
                    if (val == "none") opt.status = StatusLevel::NONE;
                    else if (val == "noxfer") opt.status = StatusLevel::NOXFER;
                    else if (val == "progress") opt.status = StatusLevel::PROGRESS;
                }
                else if (key == "conv") {
                    std::stringstream ss(val);
                    std::string conv_item;
                    while (std::getline(ss, conv_item, ',')) {
                        if (conv_item == "ucase") opt.conv_ucase = true;
                        else if (conv_item == "lcase") opt.conv_lcase = true;
                        else if (conv_item == "swab") opt.conv_swab = true;
                        else if (conv_item == "sync") opt.conv_sync = true;
                        else if (conv_item == "noerror") opt.conv_noerror = true;
                        else if (conv_item == "notrunc") opt.conv_notrunc = true;
                        else if (conv_item == "excl") opt.conv_excl = true;
                    }
                }
            }
        }
        return true;
    }
};

class DdApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]) {
        DdOptions opt;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opt, exitEarly)) {
            OptionParser::DisplayHelp(argv[0]);
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        TransferStats stats;
        if (!BlockCopier::Execute(opt, stats)) {
            return 1;
        }

        if (opt.output_format || !opt.pipe_command.empty()) {
            std::string text = (opt.output_format == 1)
                ? "{\"status\":\"success\",\"bytes\":" + std::to_string(stats.total_bytes) + "}\n"
                : (opt.output_format == 2)
                ? "status,bytes\nsuccess," + std::to_string(stats.total_bytes) + "\n"
                : "STATUS\tBYTES\nsuccess\t" + std::to_string(stats.total_bytes) + "\n";
            if (!opt.pipe_command.empty()) {
                FILE* pipe = _popen(opt.pipe_command.c_str(), "w");
                if (!pipe) return 1;
                fwrite(text.data(), 1, text.size(), pipe);
                _pclose(pipe);
            } else {
                std::cout << text;
            }
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    DdApplication app;
    return app.Run(argc, argv);
}
