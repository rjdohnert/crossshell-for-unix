#include "true_options.hpp"
#include "true_reporter.hpp"

std::string TrueReporter::wideToUtf8(const std::wstring& text) {
        if (text.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

int TrueReporter::report(const TrueOptions& opts) {
        if (opts.outputFormat != 0 || !opts.pipeCommand.empty()) {
            std::wstring text = (opts.outputFormat == 1) ? L"{\"status\":\"success\",\"exit_code\":0}\n"
                              : (opts.outputFormat == 2) ? L"status,exit_code\nsuccess,0\n"
                              : L"STATUS\tEXIT_CODE\nsuccess\t0\n";

            if (!opts.pipeCommand.empty()) {
                FILE* pipe = _wpopen(opts.pipeCommand.c_str(), L"w");
                if (!pipe) return 1;
                std::string narrow = wideToUtf8(text);
                std::fwrite(narrow.data(), 1, narrow.size(), pipe);
                _pclose(pipe);
            } else {
                std::wcout << text;
            }
        }
        return 0;
    }
