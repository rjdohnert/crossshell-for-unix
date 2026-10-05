enum class OutputFormat {
    None = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

class ExecOptions {
public:
    bool cleanEnv = false;
    bool loginShell = false;
    std::wstring altArg0;
    OutputFormat format = OutputFormat::None;
    std::wstring pipeCommand;
    bool showHelp = false;
    bool showVersion = false;
    int commandIndex = -1;

    bool Parse(int argc, wchar_t* argv[]) {
        int argIdx = 1;
        for (; argIdx < argc; ++argIdx) {
            std::wstring arg = argv[argIdx];
            if (arg == L"--json") { format = OutputFormat::Json; continue; }
            if (arg == L"--csv") { format = OutputFormat::Csv; continue; }
            if (arg == L"--table") { format = OutputFormat::Table; continue; }
            if (arg == L"--pipe" && argIdx + 1 < argc) { pipeCommand = argv[++argIdx]; continue; }
            
            if (arg == L"--help") {
                showHelp = true;
                return true;
            } else if (arg == L"--version") {
                showVersion = true;
                return true;
            } else if (arg == L"--") {
                argIdx++;
                break;
            } else if (arg == L"-c" || arg == L"--clear") {
                cleanEnv = true;
            } else if (arg == L"-l" || arg == L"--login") {
                loginShell = true;
            } else if (arg == L"-a" || arg == L"--name") {
                if (argIdx + 1 < argc) {
                    altArg0 = argv[++argIdx];
                } else {
                    std::wcerr << L"exec: option requires an argument -- " << (arg == L"-a" ? L"a" : L"name") << L"\n";
                    return false;
                }
            } else if (arg.rfind(L"--name=", 0) == 0) {
                altArg0 = arg.substr(7);
            } else if (!arg.empty() && arg[0] == L'-' && arg.length() > 1) {
                bool valid = true;
                size_t i = 1;
                while (i < arg.length()) {
                    if (arg[i] == L'c') {
                        cleanEnv = true;
                        ++i;
                    } else if (arg[i] == L'l') {
                        loginShell = true;
                        ++i;
                    } else if (arg[i] == L'a') {
                        if (i + 1 < arg.length()) {
                            altArg0 = arg.substr(i + 1);
                            break;
                        }
                        if (argIdx + 1 < argc) {
                            altArg0 = argv[++argIdx];
                        } else {
                            std::wcerr << L"exec: option requires an argument -- a\n";
                            return false;
                        }
                        break;
                    } else {
                        valid = false;
                        break;
                    }
                }
                if (!valid) {
                    std::wcerr << L"exec: invalid option -- " << arg << L"\n";
                    PrintUsage();
                    return false;
                }
            } else {
                break;
            }
        }

        commandIndex = argIdx;
        return true;
    }

    void PrintUsage() const {
        std::wcout << LR"(exec(1)                 CrossShell for UNIX Reference Manual                  exec(1)

    NAME
        exec - replace current process image or execute clean process

    SYNOPSIS
        exec [OPTIONS] COMMAND [ARGUMENTS...]

    DESCRIPTION
        exec executes COMMAND in place of the current process or with a clean,
        isolated environment.

    OPTIONS
        -c, --clear
            Execute with an empty environment.

        -l, --login
            Pass 0th argument with leading hyphen for login shell simulation.

        --json, --csv, --table
            Format launch diagnostics in structured format.

        --pipe COMMAND
            Stream output to COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Output version information and exit.

    EXAMPLES
        exec -c cmd.exe
            Replace shell with a clean environment cmd.exe session.

    CrossShell for UNIX                                                   exec(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"exec v1.0.0\n";
    }
};
