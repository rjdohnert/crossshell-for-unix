static void print_help() {
    std::wcout
        << L"Usage: expr EXPRESSION\n"
        << L"Evaluate EXPRESSION and print the result.\n\n"
        << L"Supported operators:\n"
        << L"  |  &  =  !=  <  <=  >  >=  +  -  *  /  %\n"
        << L"Parentheses are supported.\n\n"
        << L"Exit status is 0 if result is neither null nor 0; otherwise 1.\n"
        << L"  --help      display this help and exit\n"
        << L"  --version   output version information and exit\n"
        << L"  --json, --csv, --table  format the expression result\n"
        << L"  --pipe COMMAND          send the result through COMMAND\n";
}

static void print_version() {
    std::wcout << L"expr (CrossShellUX) 1.0.0\n";
}

static std::string to_utf8(const std::wstring& text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &result[0], size, nullptr, nullptr);
    return result;
}
