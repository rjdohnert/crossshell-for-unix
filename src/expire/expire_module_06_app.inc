class ExpireApplication {
public:
    int run(int argc, wchar_t* argv[]) const {
        int result = expire_main(argc, argv);
        if (g_expire_format || !g_expire_pipe.empty()) {
            std::wstring text = g_expire_format == 1 ? L"{\"status\":\"completed\",\"exit_code\":" + std::to_wstring(result) + L"}\n" : g_expire_format == 2 ? L"status,exit_code\ncompleted," + std::to_wstring(result) + L"\n" : L"STATUS\tEXIT_CODE\ncompleted\t" + std::to_wstring(result) + L"\n";
            if (!g_expire_pipe.empty()) { FILE* pipe = _wpopen(g_expire_pipe.c_str(), L"w"); if (!pipe) return EXIT_INTERNAL_ERROR; std::string narrow = expire_to_utf8(text); fwrite(narrow.data(), 1, narrow.size(), pipe); _pclose(pipe); }
            else std::wcout << text;
        }
        return result;
    }
};

