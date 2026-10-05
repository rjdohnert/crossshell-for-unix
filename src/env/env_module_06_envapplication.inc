// ============================================================================
// Core Application Controller
// ============================================================================

class EnvApplication {
public:
    explicit EnvApplication(ProgramOptions options)
        : m_opts(std::move(options)) {}

    int run() {
        if (m_opts.showHelp) {
            printHelp();
            return 0;
        }

        if (m_opts.showVersion) {
            printVersion();
            return 0;
        }

        EnvironmentManager envMgr;

        if (!m_opts.ignoreEnvironment) {
            envMgr.loadSystemEnvironment();
        }

        for (const auto& unsetVar : m_opts.unsetVars) {
            if (m_opts.verbose) {
                std::wcerr << L"env: unset: " << unsetVar << L"\n";
            }
            envMgr.unset(unsetVar);
        }

        for (const auto& [key, val] : m_opts.setVars) {
            if (m_opts.verbose) {
                std::wcerr << L"env: set: " << key << L"=" << val << L"\n";
            }
            envMgr.set(key, val);
        }

        // If no command is provided, print the environment to stdout
        if (m_opts.commandAndArgs.empty()) {
            wchar_t delimiter = m_opts.nullDelimiter ? L'\0' : L'\n';
            std::wostringstream captured;
            envMgr.printEnvironment(captured, delimiter);
            std::wstring data = captured.str();
            std::wstring text = m_opts.outputFormat == 1 ? L"{\"environment\":\"" + data + L"\"}\n" : m_opts.outputFormat == 2 ? L"\"environment\"\n\"" + data + L"\"\n" : L"ENVIRONMENT\n-----------\n" + data;
            if (!m_opts.pipeCommand.empty()) { FILE* pipe = _wpopen(m_opts.pipeCommand.c_str(), L"w"); if (!pipe) return 1; std::string narrow = env_utf8(text); fwrite(narrow.data(), 1, narrow.size(), pipe); _pclose(pipe); }
            else if (m_opts.outputFormat) std::wcout << text;
            else std::wcout << data;
            return 0;
        }

        // Execute command
        auto envBlock = envMgr.createEnvironmentBlock();
        return ProcessRunner::execute(
            m_opts.commandAndArgs,
            envBlock,
            m_opts.changeDir,
            m_opts.verbose
        );
    }

private:
    ProgramOptions m_opts;

    static void printVersion() {
        std::wcout << L"env (Windows Native Coreutils) 2.0.0 (x86_64-pc-windows-msvc)\n";
        std::wcout << L"C++17 Object-Oriented Process & Environment Utility\n";
        std::wcout << L"Full pipeline support for Windows CMD, PowerShell, and Unix emulators.\n";
    }

    static void printHelp() {
        std::wcout << LR"(Usage: env [OPTION]... [-] [NAME=VALUE]... [COMMAND [ARG]...]
Set each NAME to VALUE in the environment and run COMMAND.

Mandatory arguments to long options are mandatory for short options too.
  -i, -, --ignore-environment  start with an empty environment
  -0, --null                   end each output line with NUL, not newline
  -u, --unset=NAME             remove variable from the environment
  -C, --chdir=DIR              change working directory to DIR before execution
  -S, --split-string=S         process and split S into separate arguments;
                                 used for passing multiple arguments in shebangs
  -v, --verbose                print verbose diagnostics for each operation
    --json                       output environment as JSON
    --csv                        output environment as CSV
    --table                      output environment as a table
    --pipe COMMAND               send environment output through COMMAND
  -?, --help                   display this comprehensive help manual and exit
      --version                output version information and exit

A mere - implies -i. If no COMMAND is specified, the resulting environment
is printed to standard output.

Piping and Interoperability:
  - Standard Handles: When running COMMAND, standard input, standard output,
    and standard error are directly inherited. This allows full bidirectional
    piping (e.g. 'type file.txt | env VAR=1 cmd | findstr ...').
  - Null Delimiter: When dumping variables without a command, the -0 / --null flag
    outputs NUL-terminated key=value pairs, enabling safe piping into tools
    like xargs -0 or PowerShell splits without whitespace collisions.
  - Case-Insensitivity: Matches Windows environment semantics (variable lookup
    is case-preserving but case-insensitive).

Exit Status:
  0    if COMMAND was not specified and the environment was printed successfully
  126  if COMMAND was found but could not be invoked (e.g. permission denied)
  127  if COMMAND could not be found
  N    the exit status of COMMAND otherwise

Examples:
  1. Print current environment:
     env

  2. Print environment terminated by null characters (for piping):
     env -0

  3. Run a program with an empty environment:
     env -i powershell.exe

  4. Run a program with an altered variable:
     env NODE_ENV=production node app.js

  5. Unset variables and change working directory:
     env -u TEMP -u TMP -C C:\Projects\Build my_build_tool.exe --release

  6. Pipe data through a modified environment:
     echo test | env LOG_LEVEL=DEBUG findstr "test"

  7. Split-string parameter passing:
     env -S"FOO=BAR DEBUG=1" cmd.exe /c set
)";
    }
};
