#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "env_application.hpp"
#include "env_arguments.hpp"

#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include <exception>
#include <iostream>
#include <string>
#include <utility>

namespace {

std::wstring Utf8ToWide(const char* text) {
    if (!text) return L"unknown error";
    const int length = MultiByteToWideChar(
        CP_UTF8, 0, text, -1, nullptr, 0);
    if (length <= 0) return L"unknown error";
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), length)) {
        return L"unknown error";
    }
    result.pop_back();
    return result;
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    if (_setmode(_fileno(stdout), _O_U8TEXT) == -1 ||
        _setmode(_fileno(stderr), _O_U8TEXT) == -1) {
        std::wcerr << L"env: cannot configure UTF-8 output.\n";
        return 1;
    }

    try {
        ParseResult parsed = CommandLineParser::Parse(argc, argv);
        if (!parsed.success) {
            std::wcerr << L"env: " << parsed.error
                       << L"\nTry 'env --help' for more information.\n";
            return 2;
        }
        EnvApplication application(std::move(parsed.options));
        return application.Run();
    } catch (const std::exception& exception) {
        std::wcerr << L"env: fatal error: " << Utf8ToWide(exception.what())
                   << L'\n';
        return 1;
    } catch (...) {
        std::wcerr << L"env: unknown fatal error.\n";
        return 1;
    }
}
