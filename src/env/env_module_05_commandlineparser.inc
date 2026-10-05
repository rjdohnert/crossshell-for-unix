// ============================================================================
// Command Line Parser
// ============================================================================

class CommandLineParser {
public:
    static ProgramOptions parse(int argc, wchar_t* argv[]) {
        std::vector<std::wstring> rawArgs;
        for (int i = 1; i < argc; ++i) {
            rawArgs.push_back(argv[i]);
        }
        return parseTokens(rawArgs);
    }

    static ProgramOptions parseTokens(const std::vector<std::wstring>& args) {
        ProgramOptions opts;
        size_t idx = 0;

        while (idx < args.size()) {
            const std::wstring& arg = args[idx];

            if (arg == L"--") {
                idx++;
                break;
            }

            if (arg == L"-") {
                opts.ignoreEnvironment = true;
                idx++;
                continue;
            }

            if (arg == L"-i" || arg == L"--ignore-environment") {
                opts.ignoreEnvironment = true;
                idx++;
                continue;
            }
            if (arg == L"--json" || arg == L"--csv" || arg == L"--table") {
                opts.outputFormat = arg == L"--json" ? 1 : (arg == L"--csv" ? 2 : 3);
                idx++;
                continue;
            }
            if (arg == L"--pipe" && idx + 1 < args.size()) {
                opts.pipeCommand = args[++idx];
                idx++;
                continue;
            }

            if (arg == L"-0" || arg == L"--null") {
                opts.nullDelimiter = true;
                idx++;
                continue;
            }

            if (arg == L"-v" || arg == L"--verbose") {
                opts.verbose = true;
                idx++;
                continue;
            }

            if (arg == L"-?" || arg == L"--help") {
                opts.showHelp = true;
                idx++;
                return opts;
            }

            if (arg == L"--version") {
                opts.showVersion = true;
                idx++;
                return opts;
            }

            if (arg.rfind(L"-u", 0) == 0 || arg.rfind(L"--unset", 0) == 0) {
                std::wstring name;
                if (arg.rfind(L"--unset=", 0) == 0) {
                    name = arg.substr(8);
                } else if (arg == L"-u" || arg == L"--unset") {
                    if (idx + 1 < args.size()) {
                        name = args[++idx];
                    }
                } else if (arg.rfind(L"-u", 0) == 0 && arg.length() > 2) {
                    name = arg.substr(2);
                }
                if (!name.empty()) {
                    opts.unsetVars.push_back(name);
                }
                idx++;
                continue;
            }

            if (arg.rfind(L"-C", 0) == 0 || arg.rfind(L"--chdir", 0) == 0) {
                std::wstring dir;
                if (arg.rfind(L"--chdir=", 0) == 0) {
                    dir = arg.substr(8);
                } else if (arg == L"-C" || arg == L"--chdir") {
                    if (idx + 1 < args.size()) {
                        dir = args[++idx];
                    }
                } else if (arg.rfind(L"-C", 0) == 0 && arg.length() > 2) {
                    dir = arg.substr(2);
                }
                opts.changeDir = dir;
                idx++;
                continue;
            }

            if (arg.rfind(L"-S", 0) == 0 || arg.rfind(L"--split-string", 0) == 0) {
                std::wstring splitStr;
                if (arg.rfind(L"--split-string=", 0) == 0) {
                    splitStr = arg.substr(15);
                } else if (arg == L"-S" || arg == L"--split-string") {
                    if (idx + 1 < args.size()) {
                        splitStr = args[++idx];
                    }
                } else if (arg.rfind(L"-S", 0) == 0 && arg.length() > 2) {
                    splitStr = arg.substr(2);
                }
                
                auto expanded = ArgumentTokenizer::splitString(splitStr);
                std::vector<std::wstring> remaining(args.begin() + idx + 1, args.end());
                expanded.insert(expanded.end(), remaining.begin(), remaining.end());
                
                // Re-parse with expanded tokens
                auto subOpts = parseTokens(expanded);
                if (opts.ignoreEnvironment) subOpts.ignoreEnvironment = true;
                if (opts.nullDelimiter) subOpts.nullDelimiter = true;
                if (opts.verbose) subOpts.verbose = true;
                for (const auto& u : opts.unsetVars) subOpts.unsetVars.push_back(u);
                for (const auto& s : opts.setVars) subOpts.setVars.push_back(s);
                return subOpts;
            }

            // Check for NAME=VALUE definitions
            size_t eqPos = arg.find(L'=');
            if (eqPos != std::wstring::npos && eqPos > 0) {
                opts.setVars.emplace_back(arg.substr(0, eqPos), arg.substr(eqPos + 1));
                idx++;
                continue;
            }

            // Once we encounter a non-option, non-assignment argument, it is the COMMAND
            break;
        }

        while (idx < args.size()) {
            opts.commandAndArgs.push_back(args[idx++]);
        }

        return opts;
    }
};
