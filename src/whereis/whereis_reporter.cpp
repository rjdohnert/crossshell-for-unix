#include "target_result.hpp"
#include "whereis_reporter.hpp"

std::string WhereisReporter::toUtf8(const std::wstring& text) {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, NULL, NULL);
        return result;
    }

int WhereisReporter::dispatch(const std::vector<TargetResult>& results, int format, const std::wstring& pipeCommand) {
        std::wstring text;
        if (format == 1) {
            text = L"{\"whereis\":[";
            for (size_t i = 0; i < results.size(); ++i) {
                if (i > 0) text += L",";
                text += L"{\"target\":\"" + results[i].target + L"\"";
                text += L",\"bin\":[";
                for (size_t b = 0; b < results[i].bins.size(); ++b) {
                    if (b > 0) text += L",";
                    text += L"\"" + results[i].bins[b] + L"\"";
                }
                text += L"]}";
            }
            text += L"]}\n";
        } else if (format == 2) {
            text = L"target,path\n";
            for (const auto& r : results) {
                for (const auto& b : r.bins) text += L"\"" + r.target + L"\",\"" + b + L"\"\n";
                for (const auto& m : r.mans) text += L"\"" + r.target + L"\",\"" + m + L"\"\n";
                for (const auto& s : r.srcs) text += L"\"" + r.target + L"\",\"" + s + L"\"\n";
            }
        } else if (format == 3) {
            text = L"TARGET\tPATH\n--------------------\n";
            for (const auto& r : results) {
                for (const auto& b : r.bins) text += r.target + L"\t" + b + L"\n";
                for (const auto& m : r.mans) text += r.target + L"\t" + m + L"\n";
                for (const auto& s : r.srcs) text += r.target + L"\t" + s + L"\n";
            }
        } else {
            for (const auto& r : results) {
                text += r.target + L":";
                for (const auto& b : r.bins) text += L" " + b;
                for (const auto& m : r.mans) text += L" " + m;
                for (const auto& s : r.srcs) text += L" " + s;
                text += L"\n";
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
