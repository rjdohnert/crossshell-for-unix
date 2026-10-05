#include "options.hpp"
#include "engine.hpp"
#include <iostream>
#include <sstream>

std::vector<std::string> LsuserOptions::SplitAttributes(const std::string& value) {
    std::vector<std::string> attrs;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) attrs.push_back(item);
    }
    return attrs;
}

bool LsuserOptions::Parse(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--") {
            for (++i; i < argc; ++i) {
                requestedUsers.push_back(UserAccountEnumerator::ToWide(argv[i]));
            }
            break;
        } else if (arg == "-h" || arg == "--help") {
            showHelp = true;
            return true;
        } else if (arg == "-V" || arg == "--version") {
            showVersion = true;
            return true;
        } else if (arg == "--table") {
            format = LsuserFormat::Table;
        } else if (arg == "--csv") {
            format = LsuserFormat::Csv;
        } else if (arg == "--json") {
            format = LsuserFormat::Json;
        } else if (arg == "-") {
            std::string user;
            while (std::cin >> user) requestedUsers.push_back(UserAccountEnumerator::ToWide(user));
        } else if (arg == "-a" || arg == "--attributes") {
            if (i + 1 >= argc) {
                std::cerr << "lsuser: option requires an argument -- 'a'\n";
                return false;
            }
            requestedAttributes = SplitAttributes(argv[++i]);
        } else if (arg.rfind("-", 0) == 0) {
            std::cerr << "lsuser: invalid option -- '" << arg << "'\n";
            return false;
        } else {
            requestedUsers.push_back(UserAccountEnumerator::ToWide(arg));
        }
    }

    if (requestedAttributes.empty()) {
        requestedAttributes = { "Account", "UID", "Groups", "Login", "Status" };
    }

    return true;
}

void LsuserOptions::PrintUsage(const char* /*prog*/) const {
    std::cout << R"(lsuser(1)                CrossShell for UNIX Reference Manual                 lsuser(1)

NAME
    lsuser - list local Windows user accounts

SYNOPSIS
    lsuser [OPTIONS] [USER]...

DESCRIPTION
    Lists local user accounts in an AIX-like format. Users may be selected
    as positional arguments or read from standard input.

OPTIONS
    -a, --attributes <attrs>
        Select comma-separated attributes: account, uid, fullname, home,
        groups, login, and status.

    --table
        Output an aligned table (default).

    --csv
        Output CSV.

    --json
        Output JSON.

    -
        Read user names from standard input.

    -h, --help
        Display this comprehensive reference manual and exit.

    -V, --version
        Display version information and exit.

EXAMPLES
    lsuser
        List all local user accounts.

    lsuser -a account,uid,status
        Display only account name, UID, and status columns.

    lsuser Administrator
        Display account information for Administrator only.

EXIT STATUS
    0
        Success or no matching users.
    1
        Parse, option, enumeration, or missing-argument failure.

CrossShell for UNIX                                                    lsuser(1)
)";
}

void LsuserOptions::PrintVersion() const {
    std::cout << "lsuser 1.0.0\n";
}
