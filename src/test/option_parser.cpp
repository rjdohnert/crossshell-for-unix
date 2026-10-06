#include "option_parser.hpp"
#include "test_options.hpp"

void OptionParser::PrintHelp() {
        std::wcout << L"Usage:\n"
                   << L"  test EXPRESSION\n"
                   << L"  [ EXPRESSION ]\n"
                   << L"  test --help\n"
                   << L"  test --version\n\n"
                   << L"File tests:\n"
                   << L"  -d FILE   True if FILE exists and is a directory\n"
                   << L"  -e FILE   True if FILE exists\n"
                   << L"  -f FILE   True if FILE exists and is a regular file\n"
                   << L"  -s FILE   True if FILE exists and size > 0\n"
                   << L"  -h FILE   True if FILE exists and is a symlink/reparse point\n"
                   << L"  -r FILE   True if FILE is readable\n"
                   << L"  -w FILE   True if FILE is writable\n"
                   << L"  -x FILE   True if FILE is executable\n"
                   << L"  -t FD     True if file descriptor FD (0, 1, 2) is open on a console\n"
                   << L"  F1 -nt F2 True if F1 modification date is newer than F2\n"
                   << L"  F1 -ot F2 True if F1 modification date is older than F2\n"
                   << L"  F1 -ef F2 True if F1 and F2 point to the same physical file\n\n"
                   << L"String tests:\n"
                   << L"  -z STRING True if length of STRING is zero\n"
                   << L"  -n STRING True if length of STRING is non-zero\n"
                   << L"  S1 = S2   True if strings are equal\n"
                   << L"  S1 != S2  True if strings are not equal\n\n"
                   << L"Integer tests:\n"
                   << L"  N1 -eq N2 True if N1 equals N2\n"
                   << L"  N1 -ne N2 True if N1 is not equal to N2\n"
                   << L"  N1 -gt N2 True if N1 is greater than N2\n"
                   << L"  N1 -ge N2 True if N1 is greater than or equal to N2\n"
                   << L"  N1 -lt N2 True if N1 is less than N2\n"
                   << L"  N1 -le N2 True if N1 is less than or equal to N2\n\n"
                   << L"Logical operators:\n"
                   << L"  ! EXPR    Logical NOT\n"
                   << L"  E1 -a E2  Logical AND\n"
                   << L"  E1 -o E2  Logical OR\n"
                   << L"  ( EXPR )  Group expression\n";
        std::wcout << L"  --json, --csv, --table  Format the expression result\n  --pipe COMMAND          Send formatted result through COMMAND\n";
    }

void OptionParser::PrintVersion() {
        std::wcout << L"test\n";
    }

TestOptions OptionParser::Parse(int argc, wchar_t* argv[]) const {
        TestOptions opts;
        std::wstring prog = argv[0];
        size_t slash = prog.find_last_of(L"\\/");
        std::wstring exec_name = (slash != std::wstring::npos) ? prog.substr(slash + 1) : prog;
        std::transform(exec_name.begin(), exec_name.end(), exec_name.begin(), ::towlower);

        opts.isBracket = (exec_name == L"[" || exec_name == L"[.exe");

        for (int i = 1; i < argc; ++i) {
            std::wstring token = argv[i];
            if (token == L"--json") opts.format = 1;
            else if (token == L"--csv") opts.format = 2;
            else if (token == L"--table") opts.format = 3;
            else if (token == L"--pipe" && i + 1 < argc) opts.pipeCommand = argv[++i];
            else opts.tokens.push_back(token);
        }
        return opts;
    }
