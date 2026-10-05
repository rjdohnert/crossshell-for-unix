static int expr_main(int argc, wchar_t* argv[]) {
    if (argc <= 1) {
        std::wcerr << L"expr: missing expression\n";
        return 2;
    }

    if (argc == 2) {
        std::wstring arg = argv[1] ? argv[1] : L"";
        if (arg == L"--help") {
            print_help();
            return 0;
        }
        if (arg == L"--version") {
            print_version();
            return 0;
        }
    }

    std::vector<std::wstring> tokens;
    int format = 0;
    std::wstring pipeCommand;
    for (int i = 1; i < argc; ++i) {
        std::wstring token = argv[i] ? argv[i] : L"";
        if (token == L"--json") format = 1;
        else if (token == L"--csv") format = 2;
        else if (token == L"--table") format = 3;
        else if (token == L"--pipe" && i + 1 < argc) pipeCommand = argv[++i];
        else tokens.push_back(token);
    }

    ExprParser parser(std::move(tokens));
    std::wstring result;
    std::wstring err;
    if (!parser.parse(result, err)) {
        std::wcerr << L"expr: " << err << L"\n";
        return 2;
    }

    std::wstring text = format == 1 ? L"{\"result\":\"" + result + L"\"}\n" : format == 2 ? L"result\n" + result + L"\n" : L"RESULT\n------\n" + result + L"\n";
    if (format || !pipeCommand.empty()) { if (!pipeCommand.empty()) { FILE* pipe = _wpopen(pipeCommand.c_str(), L"w"); if (!pipe) return 2; std::string narrow = to_utf8(text); fwrite(narrow.data(), 1, narrow.size(), pipe); _pclose(pipe); } else std::wcout << text; } else std::wcout << result << L"\n";
    return (result.empty() || result == L"0") ? 1 : 0;
}
