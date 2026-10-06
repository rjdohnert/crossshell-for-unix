#include "reporter.hpp"

bool OutputFormatter::isConsoleFd(int fd) {
    if (!_isatty(fd)) return false;
    HANDLE h = (fd == _fileno(stdout)) ? GetStdHandle(STD_OUTPUT_HANDLE) : GetStdHandle(STD_ERROR_HANDLE);
    if (h == INVALID_HANDLE_VALUE || h == NULL) return false;
    DWORD mode = 0;
    return GetConsoleMode(h, &mode) != 0;
}

void OutputFormatter::configureMode() {
    const int outFd = _fileno(stdout);
    const int errFd = _fileno(stderr);

    if (isConsoleFd(outFd)) {
        _setmode(outFd, _O_U16TEXT);
    } else {
        _setmode(outFd, _O_U8TEXT);
    }

    if (isConsoleFd(errFd)) {
        _setmode(errFd, _O_U16TEXT);
    } else {
        _setmode(errFd, _O_U8TEXT);
    }
}

std::wstring OutputFormatter::sanitize(const std::wstring& input) {
    std::wostringstream out;
    out << std::uppercase;

    for (wchar_t ch : input) {
        switch (ch) {
            case L'\r': out << L"\\r"; break;
            case L'\n': out << L"\\n"; break;
            case L'\t': out << L"\\t"; break;
            default:
                if (ch < 32 || ch == 127) {
                    out << L"\\x"
                        << std::hex << std::setw(2) << std::setfill(L'0')
                        << static_cast<unsigned int>(ch)
                        << std::dec;
                } else {
                    out << ch;
                }
                break;
        }
    }
    return out.str();
}

bool OutputFormatter::iequals(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::towlower(a[i]) != std::towlower(b[i])) return false;
    }
    return true;
}

bool OutputFormatter::isPathVariable(const std::wstring& name) {
    return iequals(name, L"PATH");
}

std::vector<std::wstring> OutputFormatter::splitPath(const std::wstring& value) {
    std::vector<std::wstring> entries;
    size_t start = 0;
    while (start <= value.size()) {
        size_t sep = value.find(L';', start);
        if (sep == std::wstring::npos) {
            entries.push_back(value.substr(start));
            break;
        }
        entries.push_back(value.substr(start, sep - start));
        start = sep + 1;
    }
    return entries;
}

void OutputFormatter::printPathColumns(const std::wstring& name, const std::wstring& value, bool includeName) {
    if (includeName) {
        std::wcout << name << L"=";
    }
    std::wcout.put(L'\n');

    std::vector<std::wstring> entries = splitPath(value);
    for (const auto& entry : entries) {
        std::wcout << L"  " << sanitize(entry) << L'\n';
    }
}
