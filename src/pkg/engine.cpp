#include "engine.hpp"

// ============================================================================
// ArgumentEscaper Implementation
// ============================================================================

std::wstring ArgumentEscaper::escape(const std::wstring& arg) {
    if (arg.empty()) return L"\"\"";
    if (arg.find(L' ') == std::wstring::npos &&
        arg.find(L'\t') == std::wstring::npos &&
        arg.find(L'"') == std::wstring::npos) {
        return arg;
    }

    std::wstring escaped = L"\"";
    for (wchar_t c : arg) {
        if (c == L'"') {
            escaped += L"\\\"";
        } else {
            escaped += c;
        }
    }
    escaped += L"\"";
    return escaped;
}

std::wstring ArgumentEscaper::toLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return value;
}

bool ArgumentEscaper::endsWithCi(const std::wstring& value, const std::wstring& suffix) {
    if (value.size() < suffix.size()) return false;
    return toLower(value.substr(value.size() - suffix.size())) == toLower(suffix);
}

// ============================================================================
// LocalPackageInstaller Implementation
// ============================================================================

int LocalPackageInstaller::installLocal(const std::wstring& packagePath) {
    std::wstring cmd;
    if (ArgumentEscaper::endsWithCi(packagePath, L".msi")) {
        cmd = L"msiexec /i " + ArgumentEscaper::escape(packagePath);
    } else if (ArgumentEscaper::endsWithCi(packagePath, L".msix")) {
        cmd = L"powershell -NoProfile -ExecutionPolicy Bypass -Command Add-AppxPackage -Path " + ArgumentEscaper::escape(packagePath);
    } else {
        std::wcerr << L"pkg: --local only supports .msi and .msix files\n";
        return 1;
    }

    return _wsystem(cmd.c_str());
}

// ============================================================================
// WingetBridge Implementation
// ============================================================================

int WingetBridge::execute(const PkgOptions& options) {
    std::wstring cmd = options.command;
    const auto& args = options.arguments;

    if (cmd == L"install" || cmd == L"add") {
        bool localInstall = false;
        std::vector<std::wstring> filtered;

        for (const auto& a : args) {
            if (a == L"--local") localInstall = true;
            else filtered.push_back(a);
        }

        if (localInstall) {
            if (filtered.size() != 1) {
                std::wcerr << L"pkg: --local expects exactly one file path (.msi or .msix)\n";
                return 1;
            }
            return LocalPackageInstaller::installLocal(filtered[0]);
        }

        if (filtered.empty()) {
            std::wcerr << L"pkg: missing package name\n";
            return 1;
        }

        std::wstring full = L"winget install";
        for (const auto& a : filtered) full += L" " + ArgumentEscaper::escape(a);
        return _wsystem(full.c_str());
    }

    if (cmd == L"delete" || cmd == L"remove" || cmd == L"rm") {
        if (args.empty()) {
            std::wcerr << L"pkg: missing package name\n";
            return 1;
        }
        std::wstring full = L"winget uninstall";
        for (const auto& a : args) full += L" " + ArgumentEscaper::escape(a);
        return _wsystem(full.c_str());
    }

    if (cmd == L"search") {
        if (args.empty()) {
            std::wcerr << L"pkg: missing search query\n";
            return 1;
        }
        std::wstring full = L"winget search";
        for (const auto& a : args) full += L" " + ArgumentEscaper::escape(a);
        return _wsystem(full.c_str());
    }

    if (cmd == L"upgrade") {
        std::wstring full = args.empty() ? L"winget upgrade --all" : L"winget upgrade";
        for (const auto& a : args) full += L" " + ArgumentEscaper::escape(a);
        return _wsystem(full.c_str());
    }

    if (cmd == L"info" || cmd == L"list") {
        std::wstring full = args.empty() ? L"winget list" : L"winget show";
        for (const auto& a : args) full += L" " + ArgumentEscaper::escape(a);
        return _wsystem(full.c_str());
    }

    if (cmd == L"update") {
        return _wsystem(L"winget source update");
    }

    std::wcerr << L"Unknown command: " << cmd << L"\n\n";
    PkgOptions::printHelp();
    return 1;
}
