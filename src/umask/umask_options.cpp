#include "umask_options.hpp"

bool UmaskOptions::Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--") {
                std::wstringstream wss;
                for (int j = i + 1; j < argc; ++j) {
                    std::string a = argv[j];
                    wss << std::wstring(a.begin(), a.end()) << (j + 1 < argc ? L" " : L"");
                }
                subcommand_str = wss.str();
                break;
            }

            if (arg == "-S") {
                flag_symbolic = true;
            } else if (arg == "-p") {
                flag_print_reusable = true;
            } else if (arg == "-h" || arg == "--help" || arg == "/?") {
                show_help = true;
                return true;
            } else if (arg[0] != '-') {
                new_mask_arg = arg;
            } else {
                std::cerr << "umask: unknown option '" << arg << "'\n";
                std::cerr << "Try 'umask --help' for more information.\n";
                return false;
            }
        }
        return true;
    }

void UmaskOptions::PrintHelp() const {
        std::cout << R"(umask(1)                CrossShell for UNIX Reference Manual                 umask(1)

    NAME
        umask - get or set file mode creation mask for Windows / CrossShell

    SYNOPSIS
        umask [-S] [-p] [MASK] [-- COMMAND [ARG]...]

    DESCRIPTION
        The umask utility sets the file mode creation mask of the current process
        or prints the current mask in octal or symbolic format.

    OPTIONS
        -S
            Produce symbolic output (e.g. u=rwx,g=rx,o=rx).

        -p
            Output in a form that can be reused as input (e.g., umask 0022).

        -h, --help
            Display this reference manual.

    EXAMPLES
        umask
            Print current mask in octal format.

        umask -S
            Print current mask in symbolic notation.

        umask 027
            Set file creation mask to 027.

    CrossShell for UNIX                                                  umask(1)
)";
    }
