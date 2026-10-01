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
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstdio>

#pragma comment(lib, "user32.lib") // Automatic linking for MSVC

// ============================================================================
// 1. ENCODING & STREAM CONVERTER
// ============================================================================

class EncodingConverter {
public:
    static std::wstring ConvertToWString(const std::vector<uint8_t>& raw_bytes) {
        if (raw_bytes.empty()) return L"";

        const uint8_t* data = raw_bytes.data();
        size_t size = raw_bytes.size();

        if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
            data += 3;
            size -= 3;
        }

        if (size == 0) return L"";

        int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(data), static_cast<int>(size), nullptr, 0);
        if (wlen > 0) {
            std::wstring wstr(wlen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(data), static_cast<int>(size), &wstr[0], wlen);
            return wstr;
        }

        wlen = MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(data), static_cast<int>(size), nullptr, 0);
        if (wlen > 0) {
            std::wstring wstr(wlen, L'\0');
            MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(data), static_cast<int>(size), &wstr[0], wlen);
            return wstr;
        }

        return L"";
    }
};

class StreamReader {
public:
    static std::vector<uint8_t> ReadAllStdin() {
        _setmode(_fileno(stdin), _O_BINARY);

        std::vector<uint8_t> buffer;
        constexpr size_t CHUNK_SIZE = 65536;
        uint8_t chunk[CHUNK_SIZE];

        while (true) {
            size_t bytes_read = fread(chunk, 1, CHUNK_SIZE, stdin);
            if (bytes_read == 0) break;
            buffer.insert(buffer.end(), chunk, chunk + bytes_read);
        }

        return buffer;
    }
};

// ============================================================================
// 2. CLIPBOARD MANAGER
// ============================================================================

class ClipboardManager {
public:
    static bool SetClipboardText(const std::wstring& text) {
        constexpr int MAX_RETRIES = 10;
        constexpr int RETRY_DELAY_MS = 20;

        bool opened = false;
        for (int i = 0; i < MAX_RETRIES; ++i) {
            if (OpenClipboard(nullptr)) {
                opened = true;
                break;
            }
            Sleep(RETRY_DELAY_MS);
        }

        if (!opened) {
            std::cerr << "pbcopy: Error: Could not open Windows Clipboard (locked by another process).\n";
            return false;
        }

        if (!EmptyClipboard()) {
            std::cerr << "pbcopy: Error: Could not clear Windows Clipboard.\n";
            CloseClipboard();
            return false;
        }

        size_t bytes_needed = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes_needed);
        if (!hMem) {
            std::cerr << "pbcopy: Error: Out of memory for clipboard allocation.\n";
            CloseClipboard();
            return false;
        }

        wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
        if (!pMem) {
            GlobalFree(hMem);
            CloseClipboard();
            return false;
        }

        memcpy(pMem, text.c_str(), bytes_needed);
        GlobalUnlock(hMem);

        if (SetClipboardData(CF_UNICODETEXT, hMem) == nullptr) {
            std::cerr << "pbcopy: Error: Failed to set clipboard data.\n";
            GlobalFree(hMem);
            CloseClipboard();
            return false;
        }

        CloseClipboard();
        return true;
    }
};

// ============================================================================
// 3. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

struct PbcopyOptions {
    std::wstring pasteboard = L"general";
};

class OptionParser {
public:
    static void PrintUsage() {
        std::cout << "Usage: pbcopy [-pboard {general | ruler | find | font}] [-h | --help] [-v | --version]\n"
                  << "Read standard input and place it into the Windows system clipboard.\n\n"
                  << "Options:\n"
                  << "  -pboard <name>   Specify pasteboard (general, ruler, find, font).\n"
                  << "                   (On Windows, all options map to system clipboard).\n"
                  << "  -h, --help       Display this help menu.\n"
                  << "  -v, --version    Display version information.\n"
                  << "  --               End of options; read from standard input.\n"
                  << "  -                Treat as stdin placeholder (no-op).\n";
    }

    static void PrintVersion() {
        std::cout << "pbcopy version 1.0\n";
    }

    bool Parse(int argc, wchar_t* argv[], PbcopyOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"-h" || arg == L"--help" || arg == L"-help") {
                PrintUsage();
                exitEarly = true;
                return true;
            } else if (arg == L"-v" || arg == L"--version") {
                PrintVersion();
                exitEarly = true;
                return true;
            } else if (arg == L"--") {
                break;
            } else if (arg == L"-") {
                continue;
            } else if (arg == L"-pboard") {
                if (i + 1 < argc) {
                    opts.pasteboard = argv[++i];
                } else {
                    std::cerr << "pbcopy: Error: -pboard option requires an argument.\n";
                    return false;
                }
            } else {
                std::wcerr << L"pbcopy: Invalid option: " << arg << L"\n";
                PrintUsage();
                return false;
            }
        }
        return true;
    }
};

class PbcopyApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) {
        PbcopyOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        std::vector<uint8_t> raw_bytes = StreamReader::ReadAllStdin();
        std::wstring clipboard_text = EncodingConverter::ConvertToWString(raw_bytes);

        if (!ClipboardManager::SetClipboardText(clipboard_text)) {
            return 1;
        }

        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    PbcopyApplication app;
    return app.Run(argc, argv);
}
