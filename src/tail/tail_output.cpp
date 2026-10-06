#include "tail_output.hpp"

void TailOutput::WriteBytes(const char* data, size_t size) {
        if (size == 0) return;
        DWORD written = 0;
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr) {
            WriteFile(hOut, data, static_cast<DWORD>(size), &written, nullptr);
        } else {
            std::cout.write(data, static_cast<std::streamsize>(size));
        }
    }

std::string TailOutput::Utf8(const std::wstring& value) {
        int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

void TailOutput::PrintHeader(const std::wstring& filename, bool& first_header) {
        if (!first_header) {
            WriteBytes("\n", 1);
        }
        first_header = false;
        std::string header = "==> " + (filename == L"-" ? "standard input" : Utf8(filename)) + " <==\n";
        WriteBytes(header.data(), header.size());
    }
