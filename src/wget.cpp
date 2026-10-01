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
#include <wininet.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <cctype>
#include <streambuf>

// Link the WinINet library automatically when using MSVC compiler
#pragma comment(lib, "wininet.lib")

enum class OutputFormat { Human, Json, Csv, Table };

class PipeBuffer : public std::streambuf {
    FILE* file_; char buffer_[4096];
public:
    explicit PipeBuffer(FILE* file) : file_(file) { setp(buffer_, buffer_ + sizeof(buffer_)); }
    int_type overflow(int_type ch) override { if (ch != traits_type::eof()) { *pptr() = static_cast<char>(ch); pbump(1); } return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof(); }
    int sync() override { auto n = pptr() - pbase(); if (n && std::fwrite(pbase(), 1, static_cast<size_t>(n), file_) != static_cast<size_t>(n)) return -1; setp(buffer_, buffer_ + sizeof(buffer_)); return std::fflush(file_) == 0 ? 0 : -1; }
};

class PipeSession {
    std::streambuf* old_; FILE* file_ = nullptr; PipeBuffer* buffer_ = nullptr;
public:
    explicit PipeSession(const std::string& command) : old_(std::cout.rdbuf()) { if (!command.empty() && (file_ = _popen(command.c_str(), "w"))) { buffer_ = new PipeBuffer(file_); std::cout.rdbuf(buffer_); } }
    ~PipeSession() { std::cout.flush(); std::cout.rdbuf(old_); delete buffer_; if (file_) _pclose(file_); }
};

std::string JsonQuote(const std::string& value) { std::string out = "\""; for (char ch : value) { if (ch == '"' || ch == '\\') out += '\\'; if (ch == '\n') out += 'n'; else if (ch == '\r') out += 'r'; else out += ch; } return out + "\""; }
std::string CsvQuote(const std::string& value) { std::string out = "\""; for (char ch : value) out += ch == '"' ? "\"\"" : std::string(1, ch); return out + "\""; }

// Version: 2.0

// Helper function to extract a default filename from the URL if not provided
std::string GetFilenameFromUrl(const std::string& url) {
    size_t paramPos = url.find_first_of("?#");
    std::string cleanUrl = (paramPos != std::string::npos) ? url.substr(0, paramPos) : url;

    // Strip trailing slashes
    while (cleanUrl.length() > 8 && (cleanUrl.back() == '/' || cleanUrl.back() == '\\')) {
        cleanUrl.pop_back();
    }

    size_t lastSlash = cleanUrl.find_last_of("/\\");
    if (lastSlash != std::string::npos && lastSlash > 7 && lastSlash < cleanUrl.length() - 1) {
        std::string name = cleanUrl.substr(lastSlash + 1);
        // Replace invalid Windows filename characters
        for (char& c : name) {
            if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
                c = '_';
            }
        }
        if (!name.empty()) return name;
    }
    return "downloaded_file.bin";
}

bool IsHttpUrl(const std::string& url) {
    std::string lower = url;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lower.rfind("http://", 0) == 0 || lower.rfind("https://", 0) == 0;
}

void PrintUsage(const char* prog) {
    std::cout << R"(wget(1)                 CrossShell for UNIX Reference Manual                 wget(1)

    NAME
        wget - non-interactive network downloader

    SYNOPSIS
        wget [OPTIONS] URL [OUTPUT_FILE]

    DESCRIPTION
        Download files from the World Wide Web using HTTP or HTTPS.
        If OUTPUT_FILE is omitted, the filename is inferred from the URL.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        --json
            Emit a JSON success object.

        --csv
            Emit CSV status, URL, file, and byte fields.

        --table
            Emit tab-delimited status output.

        --pipe COMMAND
            Send textual output through pipeline COMMAND.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        wget https://example.com/archive.zip
            Download archive.zip to the current directory.

        wget --json https://example.com/data.json data.json
            Download data.json and output result in JSON format.

    CrossShell for UNIX                                                      wget(1)
)";
}

int main(int argc, char* argv[]) {
    if (argc < 2 || std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help" || std::string(argv[1]) == "/?" || std::string(argv[1]) == "-?") {
        PrintUsage(argc > 0 ? argv[0] : "wget");
        return argc < 2 ? 1 : 0;
    }

    OutputFormat outputFormat = OutputFormat::Human;
    std::string pipeCommand;
    std::string url;
    std::string filename;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") { PrintUsage(argv[0]); return 0; }
        if (arg == "-v" || arg == "--version") { std::cout << "wget 2.0\n"; return 0; }
        if (arg == "--json") outputFormat = OutputFormat::Json;
        else if (arg == "--csv") outputFormat = OutputFormat::Csv;
        else if (arg == "--table") outputFormat = OutputFormat::Table;
        else if (arg == "--pipe" && i + 1 < argc) pipeCommand = argv[++i];
        else if (url.empty()) url = arg;
        else if (filename.empty()) filename = arg;
        else { std::cerr << "Error: unexpected argument " << arg << "\n"; return 1; }
    }
    if (!IsHttpUrl(url)) {
        std::cerr << "Error: Only HTTP/HTTPS URLs are supported.\n";
        return 1;
    }

    if (filename.empty()) filename = GetFilenameFromUrl(url);
    PipeSession pipeSession(pipeCommand);
    std::string tempFilename = filename + ".part";

    const DWORD timeoutMs = 30000;
    const int maxAttempts = 3;
    int redirectCount = 0;
    const int maxRedirects = 5;

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        HINTERNET hInternet = InternetOpenA(
            "Wget/1.0",                // User-Agent
            INTERNET_OPEN_TYPE_PRECONFIG, // Use registry settings for proxy config
            NULL,
            NULL,
            0
        );

        if (!hInternet) {
            std::cerr << "Error: InternetOpen failed. Error code: " << GetLastError() << "\n";
            return 1;
        }

        InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&timeoutMs, sizeof(timeoutMs));
        InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, (LPVOID)&timeoutMs, sizeof(timeoutMs));
        InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&timeoutMs, sizeof(timeoutMs));

        // Open the connection to the URL.
        HINTERNET hUrl = InternetOpenUrlA(
            hInternet,
            url.c_str(),
            NULL,
            0,
            INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_NO_AUTO_REDIRECT,
            0
        );

        if (!hUrl) {
            DWORD err = GetLastError();
            std::cerr << "Error: InternetOpenUrl failed. Error code: " << err << "\n";
            InternetCloseHandle(hInternet);
            if (attempt == maxAttempts) {
                return 1;
            }
            std::cerr << "Retrying (" << (attempt + 1) << "/" << maxAttempts << ")...\n";
            continue;
        }

        char statusCodeBuffer[16] = { 0 };
        DWORD statusCodeBufferSize = sizeof(statusCodeBuffer);
        if (!HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE, statusCodeBuffer, &statusCodeBufferSize, NULL)) {
            DWORD err = GetLastError();
            std::cerr << "Error: Unable to determine HTTP status code. Error code: " << err << "\n";
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            if (attempt == maxAttempts) {
                return 1;
            }
            std::cerr << "Retrying (" << (attempt + 1) << "/" << maxAttempts << ")...\n";
            continue;
        }

        long statusCode = std::strtol(statusCodeBuffer, nullptr, 10);

        // Handle HTTP Redirects (301, 302, 303, 307, 308)
        if (statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 308) {
            if (++redirectCount > maxRedirects) {
                std::cerr << "Error: Too many redirects.\n";
                InternetCloseHandle(hUrl);
                InternetCloseHandle(hInternet);
                return 1;
            }
            char locationBuffer[2048] = { 0 };
            DWORD locationBufferSize = sizeof(locationBuffer);
            if (HttpQueryInfoA(hUrl, HTTP_QUERY_LOCATION, locationBuffer, &locationBufferSize, NULL)) {
                url = locationBuffer;
                std::cout << "Following redirect to: " << url << "\n";
                InternetCloseHandle(hUrl);
                InternetCloseHandle(hInternet);
                attempt--; // Don't count redirects against download attempts
                continue;
            }
        }

        if (statusCode < 200 || statusCode >= 300) {
            std::cerr << "Error: Server returned HTTP status " << statusCode << ".\n";
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);

            bool isTransient = (statusCode == 429 || statusCode == 500 || statusCode == 502 || statusCode == 503 || statusCode == 504);
            if (isTransient && attempt < maxAttempts) {
                std::cerr << "Retrying (" << (attempt + 1) << "/" << maxAttempts << ")...\n";
                continue;
            }
            return 1;
        }

        // Attempt to get Content-Length header to show download size (optional)
        uint64_t contentLength = 0;
        char contentLengthBuffer[32] = { 0 };
        DWORD contentLengthBufferSize = sizeof(contentLengthBuffer);
        bool lengthAvailable = HttpQueryInfoA(
            hUrl,
            HTTP_QUERY_CONTENT_LENGTH,
            contentLengthBuffer,
            &contentLengthBufferSize,
            NULL
        );
        if (lengthAvailable) {
            contentLength = std::strtoull(contentLengthBuffer, nullptr, 10);
        }

        // Open temporary local file for writing in binary mode.
        std::ofstream outFile(tempFilename, std::ios::binary | std::ios::trunc);
        if (!outFile.is_open()) {
            std::cerr << "Error: Failed to open output file " << tempFilename << " for writing.\n";
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return 1;
        }

        if (outputFormat == OutputFormat::Human) {
            std::cout << "Downloading: " << url << "\n";
            std::cout << "Saving to  : " << filename << "\n";
        }

        char buffer[65536]; // 64 KB
        DWORD bytesRead = 0;
        size_t totalBytesDownloaded = 0;
        bool downloadSucceeded = true;

        // Read the data in chunks and write it to the file.
        while (true) {
            if (!InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead)) {
                std::cerr << "\nError: InternetReadFile failed. Error code: " << GetLastError() << "\n";
                downloadSucceeded = false;
                break;
            }

            if (bytesRead == 0) {
                break;
            }

            outFile.write(buffer, bytesRead);
            if (!outFile.good()) {
                std::cerr << "\nError: Failed to write downloaded data to file.\n";
                downloadSucceeded = false;
                break;
            }

            totalBytesDownloaded += bytesRead;

            if (outputFormat == OutputFormat::Human && lengthAvailable && contentLength > 0) {
                double percent = (static_cast<double>(totalBytesDownloaded) / contentLength) * 100.0;
                std::printf("\rProgress: %.2f%% (%zu / %llu bytes)", percent, totalBytesDownloaded, contentLength);
            } else if (outputFormat == OutputFormat::Human) {
                std::printf("\rDownloaded: %zu bytes", totalBytesDownloaded);
            }
            std::fflush(stdout);
        }

        // Clean up resources
        outFile.close();
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);

        if (!downloadSucceeded) {
            DeleteFileA(tempFilename.c_str());
            if (attempt == maxAttempts) {
                return 1;
            }
            std::cerr << "Retrying (" << (attempt + 1) << "/" << maxAttempts << ")...\n";
            continue;
        }

        if (!MoveFileExA(tempFilename.c_str(), filename.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            std::cerr << "\nError: Failed to finalize download file. Error code: " << GetLastError() << "\n";
            DeleteFileA(tempFilename.c_str());
            return 1;
        }

        if (outputFormat == OutputFormat::Json) {
            std::cout << "{\"status\":\"success\",\"url\":" << JsonQuote(url) << ",\"file\":" << JsonQuote(filename) << ",\"bytes\":" << totalBytesDownloaded << "}\n";
        } else if (outputFormat == OutputFormat::Csv) {
            std::cout << "\"status\",\"url\",\"file\",bytes\n\"success\"," << CsvQuote(url) << ',' << CsvQuote(filename) << ',' << totalBytesDownloaded << "\n";
        } else if (outputFormat == OutputFormat::Table) {
            std::cout << "STATUS\tURL\tFILE\tBYTES\nSUCCESS\t" << url << '\t' << filename << '\t' << totalBytesDownloaded << "\n";
        } else std::cout << "\nDownload complete.\n";
        return 0;
    }

    return 1;
}
