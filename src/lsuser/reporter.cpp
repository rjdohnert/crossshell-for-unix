#include "reporter.hpp"
#include "engine.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cctype>

std::wstring LsuserReporter::GetDisplayValue(const AccountRecord& account, const std::string& attribute) {
    std::string attr = attribute;
    std::transform(attr.begin(), attr.end(), attr.begin(), [](char ch) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    });

    if (attr == "account" || attr == "name") return account.name;
    if (attr == "uid") return std::to_wstring(account.uid);
    if (attr == "fullname" || attr == "full_name" || attr == "full-name") return account.fullName;
    if (attr == "home" || attr == "home_dir" || attr == "home-dir" || attr == "homedir") return account.homeDir;
    if (attr == "shell") return account.shell.empty() ? L"cmd.exe" : account.shell;
    if (attr == "comment") return account.comment;
    if (attr == "groups" || attr == "group" || attr == "group_membership") return account.groups;
    if (attr == "login" || attr == "logon" || attr == "login_time" || attr == "logon_time" || attr == "duration") return account.loginDuration;
    if (attr == "status") return account.status;
    return L"";
}

int LsuserReporter::GetColumnWidth(const std::string& attribute) {
    std::string attr = attribute;
    std::transform(attr.begin(), attr.end(), attr.begin(), [](char ch) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    });

    if (attr == "shell") return 24;
    if (attr == "groups" || attr == "group" || attr == "group_membership") return 32;
    if (attr == "login" || attr == "logon" || attr == "login_time" || attr == "logon_time" || attr == "duration") return 18;
    if (attr == "fullname" || attr == "full_name" || attr == "full-name" || attr == "comment") return 28;
    if (attr == "home" || attr == "home_dir" || attr == "home-dir" || attr == "homedir") return 24;
    if (attr == "account" || attr == "name") return 20;
    if (attr == "uid") return 8;
    if (attr == "status") return 12;
    return 20;
}

std::string LsuserReporter::ToUtf8(const std::wstring& value) {
    int n = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(n), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::string LsuserReporter::CsvEscape(const std::string& value) {
    std::string out = "\"";
    for (char c : value) out += (c == '"' ? "\"\"" : std::string(1, c));
    return out + '"';
}

std::string LsuserReporter::JsonEscape(const std::string& value) {
    std::string out;
    for (char c : value) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

void LsuserReporter::Emit(const LsuserOptions& opts, const std::vector<AccountRecord>& accounts) {
    if (opts.format == LsuserFormat::Csv) {
        std::cout << "Account,UID,Groups,Login,Status\n";
        for (const auto& a : accounts) {
            std::cout << CsvEscape(ToUtf8(a.name)) << ','
                      << a.uid << ','
                      << CsvEscape(ToUtf8(a.groups)) << ','
                      << CsvEscape(ToUtf8(a.loginDuration)) << ','
                      << CsvEscape(ToUtf8(a.status)) << '\n';
        }
        return;
    }

    if (opts.format == LsuserFormat::Json) {
        std::cout << "[\n";
        for (size_t i = 0; i < accounts.size(); ++i) {
            const auto& a = accounts[i];
            std::cout << "  {\"account\":\"" << JsonEscape(ToUtf8(a.name)) << "\","
                      << "\"uid\":" << a.uid << ","
                      << "\"groups\":\"" << JsonEscape(ToUtf8(a.groups)) << "\","
                      << "\"login\":\"" << JsonEscape(ToUtf8(a.loginDuration)) << "\","
                      << "\"status\":\"" << JsonEscape(ToUtf8(a.status)) << "\"}"
                      << (i + 1 == accounts.size() ? "\n" : ",\n");
        }
        std::cout << "]\n";
        return;
    }

    std::wcout << std::left;
    for (size_t i = 0; i < opts.requestedAttributes.size(); ++i) {
        const auto& attribute = opts.requestedAttributes[i];
        std::wcout << std::setw(GetColumnWidth(attribute)) << UserAccountEnumerator::ToWide(attribute)
                   << (i + 1 < opts.requestedAttributes.size() ? L"  " : L"");
    }
    std::wcout << std::endl;

    for (size_t i = 0; i < opts.requestedAttributes.size(); ++i) {
        const auto& attribute = opts.requestedAttributes[i];
        std::wcout << std::wstring(static_cast<size_t>(GetColumnWidth(attribute)), L'-')
                   << (i + 1 < opts.requestedAttributes.size() ? L"  " : L"");
    }
    std::wcout << std::endl;

    for (const auto& account : accounts) {
        for (size_t i = 0; i < opts.requestedAttributes.size(); ++i) {
            const auto& attribute = opts.requestedAttributes[i];
            std::wcout << std::setw(GetColumnWidth(attribute)) << GetDisplayValue(account, attribute)
                       << (i + 1 < opts.requestedAttributes.size() ? L"  " : L"");
        }
        std::wcout << std::endl;
    }
}
