#include "output_formatter.hpp"
#include "string_utils.hpp"

void OutputFormatter::Emit(int format, const std::wstring& pipeCommand, bool resume, bool all_ok) {
        if (format == 0 && pipeCommand.empty()) return;

        std::wstring text;
        if (format == 1) {
            text = L"{\"action\":\"" + std::wstring(resume ? L"resume" : L"suspend") + L"\",\"success\":" + std::wstring(all_ok ? L"true" : L"false") + L"}\n";
        } else if (format == 2) {
            text = L"action,success\n" + std::wstring(resume ? L"resume," : L"suspend,") + (all_ok ? L"true\n" : L"false\n");
        } else {
            text = L"ACTION\tSUCCESS\n" + std::wstring(resume ? L"resume\t" : L"suspend\t") + (all_ok ? L"true\n" : L"false\n");
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _wpopen(pipeCommand.c_str(), L"w");
            if (pipe) {
                std::string narrow = StringUtils::Utf8(text);
                fwrite(narrow.data(), 1, narrow.size(), pipe);
                _pclose(pipe);
            }
        } else {
            std::wcout << text;
        }
    }
