#include "output_formatter.hpp"

void OutputFormatter::Emit(int format, const std::wstring& pipeCommand, bool result) {
        if (format == 0 && pipeCommand.empty()) return;

        std::wstring text = format == 1 ? (L"{\"result\":" + std::wstring(result ? L"true" : L"false") + L"}\n") :
                            format == 2 ? (L"result\n" + std::wstring(result ? L"true\n" : L"false\n")) :
                                          (L"RESULT\n" + std::wstring(result ? L"true\n" : L"false\n"));

        if (!pipeCommand.empty()) {
            FILE* pipe = _wpopen(pipeCommand.c_str(), L"w");
            if (pipe) {
                int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
                if (size > 0) {
                    std::string narrow(static_cast<size_t>(size), '\0');
                    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), size, nullptr, nullptr);
                    fwrite(narrow.data(), 1, narrow.size(), pipe);
                }
                _pclose(pipe);
            }
        } else {
            std::wcout << text;
        }
    }
