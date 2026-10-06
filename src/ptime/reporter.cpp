#include "reporter.hpp"

std::wstring PtimeReporter::FormatSeconds(double sec) {
    wchar_t buf[64];
    swprintf_s(buf, L"%.2f", sec);
    return buf;
}

std::wstring PtimeReporter::FormatHuman(double sec) {
    int total_sec = static_cast<int>(sec);
    int minutes = total_sec / 60;
    double remaining_sec = sec - (minutes * 60);
    wchar_t buf[64];
    if (minutes > 0) {
        swprintf_s(buf, L"%dm%.3fs", minutes, remaining_sec);
    } else {
        swprintf_s(buf, L"%.3fs", remaining_sec);
    }
    return buf;
}

void PtimeReporter::Report(const TimeOptions& opts, const ProcessTimeMetrics& metrics) {
    std::wstring out_str;
    if (opts.posixFormat) {
        if (opts.humanReadable) {
            out_str += L"real " + FormatHuman(metrics.realSec) + L"\n";
            out_str += L"user " + FormatHuman(metrics.userSec) + L"\n";
            out_str += L"sys  " + FormatHuman(metrics.sysSec) + L"\n";
        } else {
            out_str += L"real " + FormatSeconds(metrics.realSec) + L"\n";
            out_str += L"user " + FormatSeconds(metrics.userSec) + L"\n";
            out_str += L"sys  " + FormatSeconds(metrics.sysSec) + L"\n";
        }
    } else {
        if (opts.humanReadable) {
            out_str += L"\t" + FormatHuman(metrics.realSec) + L" real " +
                       L"\t" + FormatHuman(metrics.userSec) + L" user " +
                       L"\t" + FormatHuman(metrics.sysSec) + L" sys\n";
        } else {
            out_str += L"\t" + FormatSeconds(metrics.realSec) + L" real " +
                       L"\t" + FormatSeconds(metrics.userSec) + L" user " +
                       L"\t" + FormatSeconds(metrics.sysSec) + L" sys\n";
        }
    }

    if (opts.detailedInfo) {
        wchar_t details[512];
        swprintf_s(details,
            L"%12zu  maximum resident set size (bytes)\n"
            L"%12zu  page faults\n"
            L"%12zu  processes created\n"
            L"%12llu  I/O read operations\n"
            L"%12llu  I/O write operations\n"
            L"%12llu  I/O bytes read\n"
            L"%12llu  I/O bytes written\n",
            metrics.peakRam, metrics.pageFaults, metrics.totalProcs,
            metrics.ioReadOps, metrics.ioWriteOps, metrics.ioReadBytes, metrics.ioWriteBytes
        );
        out_str += details;
    }

    WriteOutput(opts, out_str);
}

void PtimeReporter::WriteOutput(const TimeOptions& opts, const std::wstring& text) {
    if (!opts.outputFile.empty()) {
        std::wofstream out(opts.outputFile, opts.appendOutput ? std::ios::app : std::ios::out);
        if (out.is_open()) {
            out << text;
            return;
        }
    }

    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode;
    if (GetConsoleMode(hErr, &mode)) {
        DWORD written;
        WriteConsoleW(hErr, text.c_str(), static_cast<DWORD>(text.length()), &written, NULL);
    } else {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.length()), NULL, 0, NULL, NULL);
        if (size > 0) {
            std::string utf8_str(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.length()), &utf8_str[0], size, NULL, NULL);
            DWORD written;
            WriteFile(hErr, utf8_str.data(), static_cast<DWORD>(utf8_str.length()), &written, NULL);
        }
    }
}
