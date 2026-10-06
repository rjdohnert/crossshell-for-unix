#include "output_formatter.hpp"

std::wstring OutputFormatter::Quote(const std::wstring& value) {
        std::wstring out = L"\"";
        for (wchar_t c : value) {
            if (c == L'"' || c == L'\\') out += L'\\';
            out += c;
        }
        return out + L"\"";
    }

void OutputFormatter::EmitHeader(int format, std::wostream& out) {
        if (format == 2) out << L"status,path\n";
        else if (format == 3) out << L"STATUS\tPATH\n";
    }

void OutputFormatter::EmitRecord(const std::wstring& target, const std::wstring& status, int format, std::wostream& out) {
        if (format == 1) {
            out << L"{\"status\":\"" << status << L"\",\"path\":" << Quote(target) << L"}\n";
        } else if (format == 2) {
            out << status << L"," << Quote(target) << L"\n";
        } else if (format == 3) {
            out << status << L"\t" << target << L"\n";
        }
    }
