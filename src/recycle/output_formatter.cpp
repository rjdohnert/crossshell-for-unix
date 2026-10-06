#include "output_formatter.hpp"
#include "wide_pipe_buffer.hpp"

std::wstring OutputFormatter::CsvQuote(const std::wstring& value) {
        std::wstring out = L"\"";
        for (wchar_t ch : value) {
            out += (ch == L'"' ? L"\"\"" : std::wstring(1, ch));
        }
        return out + L"\"";
    }

std::wstring OutputFormatter::JsonQuote(const std::wstring& value) {
        std::wstring out = L"\"";
        for (wchar_t ch : value) {
            if (ch == L'"' || ch == L'\\') out += L'\\';
            if (ch == L'\n') out += L'n';
            else if (ch == L'\r') out += L'r';
            else out += ch;
        }
        return out + L"\"";
    }

void OutputFormatter::EmitHeader(OutputFormat fmt) {
        if (fmt == OutputFormat::Csv) std::wcout << L"\"status\",\"path\"\n";
        if (fmt == OutputFormat::Table) std::wcout << L"STATUS\tPATH\n";
    }

void OutputFormatter::EmitItem(OutputFormat fmt, const std::wstring& path, bool success, bool verbose, bool quiet) {
        if (success) {
            if (verbose) {
                std::wcout << L"DONE\n";
            } else if (!quiet && fmt == OutputFormat::Human) {
                std::wcout << L"Recycled: " << path << L"\n";
            }
            if (fmt == OutputFormat::Json) {
                std::wcout << L"{\"status\":\"success\",\"path\":" << JsonQuote(path) << L"}\n";
            } else if (fmt == OutputFormat::Csv) {
                std::wcout << L"\"success\"," << CsvQuote(path) << L"\n";
            } else if (fmt == OutputFormat::Table) {
                std::wcout << L"success\t" << path << L"\n";
            }
        } else {
            if (verbose) {
                std::wcout << L"FAILED\n";
            }
        }
    }

void OutputFormatter::EmitSummary(int successCount, int failureCount, bool quiet, OutputFormat fmt, size_t targetCount, bool verbose) {
        if (!quiet && fmt == OutputFormat::Human && (targetCount > 1 || verbose)) {
            std::wcout << L"\nSummary: " << successCount << L" succeeded, "
                       << failureCount << L" failed.\n";
        }
    }
