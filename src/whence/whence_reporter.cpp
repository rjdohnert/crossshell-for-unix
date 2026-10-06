#include "whence_reporter.hpp"
#include "whence_result.hpp"

std::string WhenceReporter::toUtf8(const std::wstring& text) {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, NULL, NULL);
        return result;
    }

int WhenceReporter::dispatch(const std::vector<WhenceResult>& results, int format, bool verbose, const std::wstring& pipeCommand) {
        std::wstring text;
        if (format == 1) {
            text = L"{\"whence\":[";
            for (size_t i = 0; i < results.size(); ++i) {
                if (i > 0) text += L",";
                text += L"{\"target\":\"" + results[i].target + L"\",\"type\":\"" + results[i].type + L"\",\"path\":\"" + results[i].path + L"\"}";
            }
            text += L"]}\n";
        } else if (format == 2) {
            text = L"target,type,path\n";
            for (const auto& r : results) {
                text += L"\"" + r.target + L"\",\"" + r.type + L"\",\"" + r.path + L"\"\n";
            }
        } else if (format == 3) {
            text = L"TARGET\tTYPE\tPATH\n------------------------------------\n";
            for (const auto& r : results) {
                text += r.target + L"\t" + r.type + L"\t" + r.path + L"\n";
            }
        } else {
            for (const auto& r : results) {
                if (verbose) {
                    if (r.type == L"builtin") text += r.target + L" is a shell builtin\n";
                    else if (r.type == L"not_found") text += r.target + L" not found\n";
                    else text += r.target + L" is " + r.path + L"\n";
                } else {
                    if (r.type == L"builtin") text += r.target + L"\n";
                    else if (r.type != L"not_found") text += r.path + L"\n";
                }
            }
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _wpopen(pipeCommand.c_str(), L"w");
            if (!pipe) return 1;
            std::string utf8 = toUtf8(text);
            std::fwrite(utf8.data(), 1, utf8.size(), pipe);
            _pclose(pipe);
        } else {
            std::wcout << text;
        }
        return 0;
    }
