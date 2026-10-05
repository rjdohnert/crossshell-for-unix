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
 */

#include "block_copier.hpp"
#include "data_transformer.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

void ProgressReporter::PrintStats(const TransferStats& stats, double elapsed_seconds, StatusLevel status, bool is_progress) {
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

bool BlockCopier::Execute(const DdOptions& opt, TransferStats& stats) {
#ifdef _WIN32
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
#else
    (void)opt;
    (void)stats;
    return false;
#endif
}
