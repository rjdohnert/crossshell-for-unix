#include "vhdctl_options.hpp"

bool VhdctlOptions::Parse(int argc, wchar_t* argv[]) {
        if (argc < 2) {
            showHelp = true;
            return false;
        }

        command = argv[1];
        if (command == L"-h" || command == L"--help" || command == L"help") {
            showHelp = true;
            return true;
        }
        if (command == L"--version") {
            showVersion = true;
            return true;
        }

        for (int i = 2; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"--") {
                break;
            } else if ((arg == L"-f" || arg == L"--file") && i + 1 < argc) {
                filePath = argv[++i];
            } else if ((arg == L"-s" || arg == L"--size") && i + 1 < argc) {
                sizeStr = argv[++i];
            } else if ((arg == L"-t" || arg == L"--type") && i + 1 < argc) {
                std::wstring type = argv[++i];
                if (type == L"fixed") isFixed = true;
                else if (type == L"dynamic") isFixed = false;
                else {
                    std::wcerr << L"vhdctl: invalid type '" << type << L"'" << std::endl;
                    return false;
                }
            } else if (arg == L"-r" || arg == L"--readonly") {
                readOnly = true;
            } else if (arg == L"--json" || arg == L"--csv" || arg == L"--table") {
                outputFormat = (arg == L"--json") ? OutputFormat::Json : (arg == L"--csv" ? OutputFormat::Csv : OutputFormat::Table);
            } else if (arg == L"--pipe" && i + 1 < argc) {
                pipeCommand = argv[++i];
            } else if (arg[0] == L'-') {
                std::wcerr << L"vhdctl: invalid option '" << arg << L"'" << std::endl;
                return false;
            } else {
                std::wcerr << L"vhdctl: unexpected argument '" << arg << L"'" << std::endl;
                return false;
            }
        }

        if (filePath.empty()) {
            std::wcerr << L"vhdctl: Missing required parameter -f / --file." << std::endl;
            return false;
        }

        return true;
    }
