#include "option_parser.hpp"
#include "typeset_options.hpp"
#include "variable_attributes.hpp"

void OptionParser::PrintHelp() {
        std::wcout << LR"(typeset(1)              CrossShell for UNIX Reference Manual               typeset(1)

    NAME
        typeset - declare variables, set attributes, and manage environment values

    SYNOPSIS
        typeset [OPTIONS] [NAME[=VALUE]...]
        typeset -p [NAME...]

    DESCRIPTION
        Sets attributes, formatting constraints, and values for variables in the
        current shell session, process environment, and Windows Registry. When
        invoked without arguments, typeset displays all declared variables and
        their associated qualifiers.

    OPTIONS AND QUALIFIERS
        -x, --export
            Mark variable for automatic export to the process environment.
            Using '+x' removes the export attribute.

        -r, --readonly
            Mark variable as read-only. Prevents subsequent modifications.

        -i, --integer [BASE]
            Designate variable as an integer. Arithmetic evaluation and base
            conversion (2-36) are applied upon assignment.

        -u, --uppercase
            Convert all characters in VALUE to uppercase upon assignment.

        -l, --lowercase
            Convert all characters in VALUE to lowercase upon assignment.

        -L, --left [WIDTH]
            Left-justify VALUE within a field of WIDTH characters, padding
            with trailing spaces and truncating excess characters.

        -R, --right [WIDTH]
            Right-justify VALUE within a field of WIDTH characters, padding
            with leading spaces and truncating excess characters.

        -Z, --zerofill [WIDTH]
            Right-justify and zero-fill leading positions up to WIDTH digits.

        -t, --trace, --tag
            Set the tag/trace execution attribute on the variable.

        -a, --array
            Declare variable as an indexed array.

        -A, --assoc
            Declare variable as an associative array / hash table.

        -n, --nameref
            Declare variable as an indirect name reference to another variable.

        -f, --functions
            Display or apply attributes to shell functions.

        -g, --global
            Save variable persistently to the Windows User Registry
            (HKCU\Environment) with broadcast notification.

        -p, --print
            Display variable declarations and attribute qualifiers in a
            reusable typeset command format.

    OUTPUT AND CONTROL
        --json, --csv, --table
            Format output as JSON objects, CSV records, or an aligned table.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        typeset
            Display all environment variables and attributes.

        typeset -u LOG_LEVEL=debug
            Convert value to uppercase (LOG_LEVEL="DEBUG").

        typeset -l DOMAIN_NAME=CORP.LOCAL
            Convert value to lowercase (DOMAIN_NAME="corp.local").

        typeset -i COUNT=042
            Declare an integer variable (COUNT="42").

        typeset -Z5 SEQ_ID=7
            Zero-pad value to 5 digits (SEQ_ID="00007").

        typeset -L10 CODE=abcdefghijk
            Left-justify and truncate to 10 characters (CODE="abcdefghij").

        typeset -x -g USER_HOME=C:\Users\Admin
            Set in process environment and persist to Windows Registry.

        typeset -p PATH
            Display the typeset declaration and qualifiers for PATH.

        typeset --json | jq .
            Export variable definitions as structured JSON.

    CrossShell for UNIX                                                   typeset(1)
)";
    }

void OptionParser::PrintVersion() {
        std::wcout << L"typeset (CrossShell) 5.0.0\n";
    }

bool OptionParser::Parse(int argc, wchar_t* argv[], TypesetOptions& opts) const {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                opts.showHelp = true;
                return true;
            } else if (arg == L"-V" || arg == L"--version") {
                opts.showVersion = true;
                return true;
            } else if (arg == L"--json") {
                opts.format = OutputFormat::Json;
            } else if (arg == L"--csv") {
                opts.format = OutputFormat::Csv;
            } else if (arg == L"--table") {
                opts.format = OutputFormat::Table;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg == L"-p" || arg == L"--print") {
                opts.optPrint = true;
            } else if (arg == L"-x" || arg == L"--export") {
                opts.attributes.isExport = true;
            } else if (arg == L"+x") {
                opts.attributes.isExport = false;
            } else if (arg == L"-r" || arg == L"--readonly") {
                opts.attributes.isReadOnly = true;
            } else if (arg == L"+r") {
                opts.attributes.isReadOnly = false;
            } else if (arg == L"-u" || arg == L"--uppercase") {
                opts.attributes.isUpper = true;
                opts.attributes.isLower = false;
            } else if (arg == L"+u") {
                opts.attributes.isUpper = false;
            } else if (arg == L"-l" || arg == L"--lowercase") {
                opts.attributes.isLower = true;
                opts.attributes.isUpper = false;
            } else if (arg == L"+l") {
                opts.attributes.isLower = false;
            } else if (arg.rfind(L"-i", 0) == 0 || arg == L"--integer") {
                opts.attributes.isInteger = true;
                if (arg.length() > 2 && arg != L"--integer") {
                    try { opts.attributes.integerBase = std::stoi(arg.substr(2)); } catch (...) {}
                }
            } else if (arg == L"+i") {
                opts.attributes.isInteger = false;
            } else if (arg.rfind(L"-L", 0) == 0 || arg == L"--left") {
                opts.attributes.isLeftJustify = true;
                if (arg.length() > 2 && arg != L"--left") {
                    try { opts.attributes.leftWidth = static_cast<size_t>(std::stoul(arg.substr(2))); } catch (...) {}
                } else if (arg == L"--left" && i + 1 < argc && iswdigit(argv[i+1][0])) {
                    opts.attributes.leftWidth = static_cast<size_t>(std::stoul(argv[++i]));
                }
            } else if (arg == L"+L") {
                opts.attributes.isLeftJustify = false;
            } else if (arg.rfind(L"-R", 0) == 0 || arg == L"--right") {
                opts.attributes.isRightJustify = true;
                if (arg.length() > 2 && arg != L"--right") {
                    try { opts.attributes.rightWidth = static_cast<size_t>(std::stoul(arg.substr(2))); } catch (...) {}
                } else if (arg == L"--right" && i + 1 < argc && iswdigit(argv[i+1][0])) {
                    opts.attributes.rightWidth = static_cast<size_t>(std::stoul(argv[++i]));
                }
            } else if (arg == L"+R") {
                opts.attributes.isRightJustify = false;
            } else if (arg.rfind(L"-Z", 0) == 0 || arg == L"--zerofill") {
                opts.attributes.isZeroFill = true;
                if (arg.length() > 2 && arg != L"--zerofill") {
                    try { opts.attributes.zeroWidth = static_cast<size_t>(std::stoul(arg.substr(2))); } catch (...) {}
                } else if (arg == L"--zerofill" && i + 1 < argc && iswdigit(argv[i+1][0])) {
                    opts.attributes.zeroWidth = static_cast<size_t>(std::stoul(argv[++i]));
                }
            } else if (arg == L"+Z") {
                opts.attributes.isZeroFill = false;
            } else if (arg == L"-t" || arg == L"--tag" || arg == L"--trace") {
                opts.attributes.isTagged = true;
            } else if (arg == L"+t") {
                opts.attributes.isTagged = false;
            } else if (arg == L"-a" || arg == L"--array") {
                opts.attributes.isArray = true;
            } else if (arg == L"+a") {
                opts.attributes.isArray = false;
            } else if (arg == L"-A" || arg == L"--assoc") {
                opts.attributes.isAssoc = true;
            } else if (arg == L"+A") {
                opts.attributes.isAssoc = false;
            } else if (arg == L"-n" || arg == L"--nameref") {
                opts.attributes.isNameRef = true;
            } else if (arg == L"+n") {
                opts.attributes.isNameRef = false;
            } else if (arg == L"-f" || arg == L"--functions") {
                opts.attributes.isFunction = true;
            } else if (arg == L"+f") {
                opts.attributes.isFunction = false;
            } else if (arg == L"-g" || arg == L"--global") {
                opts.attributes.isGlobal = true;
            } else if (arg == L"+g") {
                opts.attributes.isGlobal = false;
            } else if (arg == L"--") {
                while (++i < argc) opts.targets.push_back(argv[i]);
                break;
            } else {
                opts.targets.push_back(arg);
            }
        }
        return true;
    }
