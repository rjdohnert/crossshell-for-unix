#include "output_formatter.hpp"
#include "string_utils.hpp"

OutputFormatter::OutputFormatter(OutputFormat fmt, std::wstring pipeCmd)
        : m_format(fmt), m_pipeCommand(std::move(pipeCmd)) {}

void OutputFormatter::Emit(const std::wstring& message) const {
        std::wstring text;
        switch (m_format) {
            case OutputFormat::Json:
                text = L"{\"status\":\"success\",\"message\":\"" + message + L"\"}\n";
                break;
            case OutputFormat::Csv:
                text = L"status,message\nsuccess,\"" + message + L"\"\n";
                break;
            case OutputFormat::Table:
                text = L"STATUS\tMESSAGE\nsuccess\t" + message + L"\n";
                break;
            default:
                text = message + L"\n";
                break;
        }

        if (!m_pipeCommand.empty()) {
            FILE* p = _wpopen(m_pipeCommand.c_str(), L"w");
            if (p) {
                std::string n = StringUtils::Utf8(text);
                fwrite(n.data(), 1, n.size(), p);
                _pclose(p);
            }
        } else {
            std::wcout << text;
        }
    }
