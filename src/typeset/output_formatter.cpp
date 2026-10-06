#include "output_formatter.hpp"
#include "string_utils.hpp"
#include "variable_attributes.hpp"

OutputFormatter::OutputFormatter(OutputFormat fmt) : m_format(fmt) {}

void OutputFormatter::EmitRecord(const std::wstring& name, const std::wstring& value, const VariableAttributes& attr ) {
        switch (m_format) {
            case OutputFormat::Json:
                std::wcout << L"{\"name\":\"" << name << L"\",\"value\":\"" << value << L"\"}\n";
                break;
            case OutputFormat::Csv:
                if (!m_headerEmitted) {
                    std::wcout << L"\"name\",\"value\"\n";
                    m_headerEmitted = true;
                }
                std::wcout << L"\"" << name << L"\",\"" << value << L"\"\n";
                break;
            case OutputFormat::Table:
                if (!m_headerEmitted) {
                    std::wcout << L"NAME\tVALUE\n";
                    m_headerEmitted = true;
                }
                std::wcout << name << L"\t" << value << L"\n";
                break;
            default:
                std::wcout << L"typeset" << StringUtils::FormatFlags(attr) << L" " << name << L"=\"" << value << L"\"\n";
                break;
        }
    }
