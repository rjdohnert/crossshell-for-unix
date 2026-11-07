#include "env_output.hpp"

#include <algorithm>
#include <cwctype>
#include <ostream>
#include <string>

namespace {

void AppendHex4(std::wstring& output, unsigned int value) {
    static constexpr wchar_t digits[] = L"0123456789ABCDEF";
    output += L"\\u";
    output.push_back(digits[(value >> 12) & 0x0f]);
    output.push_back(digits[(value >> 8) & 0x0f]);
    output.push_back(digits[(value >> 4) & 0x0f]);
    output.push_back(digits[value & 0x0f]);
}

std::wstring JsonString(std::wstring_view value) {
    std::wstring escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back(L'"');
    for (std::size_t index = 0; index < value.size(); ++index) {
        const wchar_t character = value[index];
        switch (character) {
            case L'"': escaped += L"\\\""; continue;
            case L'\\': escaped += L"\\\\"; continue;
            case L'\b': escaped += L"\\b"; continue;
            case L'\f': escaped += L"\\f"; continue;
            case L'\n': escaped += L"\\n"; continue;
            case L'\r': escaped += L"\\r"; continue;
            case L'\t': escaped += L"\\t"; continue;
            default: break;
        }

        const unsigned int code = static_cast<unsigned int>(character);
        if (code < 0x20) {
            AppendHex4(escaped, code);
        } else if (code >= 0xd800 && code <= 0xdbff) {
            if (index + 1 < value.size()) {
                const unsigned int next =
                    static_cast<unsigned int>(value[index + 1]);
                if (next >= 0xdc00 && next <= 0xdfff) {
                    escaped.push_back(character);
                    escaped.push_back(value[++index]);
                    continue;
                }
            }
            AppendHex4(escaped, code);
        } else if (code >= 0xdc00 && code <= 0xdfff) {
            AppendHex4(escaped, code);
        } else {
            escaped.push_back(character);
        }
    }
    escaped.push_back(L'"');
    return escaped;
}

bool IsFormulaLike(std::wstring_view value) {
    std::size_t index = 0;
    while (index < value.size() && iswspace(value[index])) ++index;
    if (index == value.size()) return false;
    const wchar_t first = value[index];
    return first == L'=' || first == L'+' || first == L'-' ||
           first == L'@';
}

std::wstring CsvField(std::wstring_view value) {
    std::wstring escaped;
    escaped.reserve(value.size() + 3);
    if (IsFormulaLike(value)) escaped.push_back(L'\'');
    for (wchar_t character : value) {
        if (character == L'"') escaped.push_back(L'"');
        escaped.push_back(character);
    }
    return L"\"" + escaped + L"\"";
}

std::wstring TableCell(std::wstring_view value) {
    std::wstring escaped;
    for (wchar_t character : value) {
        switch (character) {
            case L'\\': escaped += L"\\\\"; break;
            case L'|': escaped += L"\\|"; break;
            case L'\n': escaped += L"\\n"; break;
            case L'\r': escaped += L"\\r"; break;
            case L'\t': escaped += L"\\t"; break;
            default: {
                const unsigned int code =
                    static_cast<unsigned int>(character);
                if (code < 0x20 || code == 0x7f ||
                    code == 0x2028 || code == 0x2029) {
                    static constexpr wchar_t digits[] =
                        L"0123456789ABCDEF";
                    escaped += code > 0xff ? L"\\u" : L"\\x";
                    escaped.push_back(digits[(code >> 12) & 0x0f]);
                    escaped.push_back(digits[(code >> 8) & 0x0f]);
                    escaped.push_back(digits[(code >> 4) & 0x0f]);
                    escaped.push_back(digits[code & 0x0f]);
                } else {
                    escaped.push_back(character);
                }
            }
        }
    }
    return escaped;
}

bool Write(const TextSink& sink, std::wstring_view text,
           std::wstring& error) {
    if (sink(text)) return true;
    error = L"failed while writing environment output.";
    return false;
}

} // namespace

bool WriteEnvironment(const EnvironmentManager::Map& environment,
                      OutputFormat format, bool nullDelimiter,
                      const TextSink& sink, std::wstring& error) {
    if (format == OutputFormat::Json) {
        if (!Write(sink, L"{\"environment\":{", error)) return false;
        bool first = true;
        for (const auto& [name, value] : environment) {
            if (!Write(sink, first ? L"" : L",", error) ||
                !Write(sink, JsonString(name), error) ||
                !Write(sink, L":", error) ||
                !Write(sink, JsonString(value), error)) {
                return false;
            }
            first = false;
        }
        return Write(sink, L"}}\n", error);
    }

    if (format == OutputFormat::Csv) {
        if (!Write(sink, L"Name,Value\n", error)) return false;
        for (const auto& [name, value] : environment) {
            if (!Write(sink, CsvField(name), error) ||
                !Write(sink, L",", error) ||
                !Write(sink, CsvField(value), error) ||
                !Write(sink, L"\n", error)) {
                return false;
            }
        }
        return true;
    }

    if (format == OutputFormat::Table) {
        std::size_t nameWidth = 4;
        for (const auto& [name, value] : environment) {
            (void)value;
            nameWidth = (std::max)(nameWidth, TableCell(name).size());
        }
        const std::wstring header = L"NAME" +
            std::wstring(nameWidth - 4, L' ') + L" | VALUE\n";
        const std::wstring separator =
            std::wstring(nameWidth, L'-') + L"-+-------\n";
        if (!Write(sink, header, error) ||
            !Write(sink, separator, error)) {
            return false;
        }
        for (const auto& [name, value] : environment) {
            const std::wstring safeName = TableCell(name);
            const std::wstring row = safeName +
                std::wstring(nameWidth - safeName.size(), L' ') + L" | " +
                TableCell(value) + L"\n";
            if (!Write(sink, row, error)) return false;
        }
        return true;
    }

    const wchar_t delimiter = nullDelimiter ? L'\0' : L'\n';
    for (const auto& [name, value] : environment) {
        if (!Write(sink, name, error) ||
            !Write(sink, L"=", error) ||
            !Write(sink, value, error) ||
            !Write(sink, std::wstring_view(&delimiter, 1), error)) {
            return false;
        }
    }
    return true;
}

void PrintHelp(std::wostream& output) {
    output << LR"(Usage: env [OPTION]... [-] [NAME=VALUE]... [COMMAND [ARG]...]
Set environment variables and optionally run COMMAND.

  -i, -, --ignore-environment  start with an empty environment
  -0, --null                   end environment entries with NUL
  -u, --unset NAME             remove a variable
      --unset=NAME
  -C, --chdir DIR              run COMMAND from DIR
      --chdir=DIR
  -S, --split-string STRING    split STRING into command arguments
      --json                   print the environment as JSON
      --csv                    print the environment as CSV
      --table                  print the environment as an escaped table
      --pipe COMMAND           send the environment listing to COMMAND
  -v, --verbose                print operation diagnostics
  -?, --help                   display this help
      --version                display version information
  --                           treat remaining arguments as COMMAND and its arguments

With no COMMAND, the resulting environment is printed. Structured output and
--pipe apply only to environment listings. --null cannot be combined with a
structured format.

Examples:
  env
  env -0
  env -i powershell.exe
  env NODE_ENV=production node app.js
  env -u TEMP -C C:\Projects\Build build.exe --release
  env -S "FOO=BAR cmd.exe /c set"
)";
}

void PrintVersion(std::wostream& output) {
    output << L"env (CrossShell for UNIX) 2.1.0\n";
}
