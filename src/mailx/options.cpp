#include "options.hpp"
#include <iostream>
#include <cwctype>

std::wstring MailOptions::Trim(const std::wstring& s) {
    size_t b = 0;
    while (b < s.size() && iswspace(s[b])) {
        ++b;
    }
    size_t e = s.size();
    while (e > b && iswspace(s[e - 1])) {
        --e;
    }
    return s.substr(b, e - b);
}

bool MailOptions::Parse(int argc, wchar_t* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";

        if (arg == L"--help" || arg == L"-h") {
            showHelp = true;
            return true;
        }
        if (arg == L"--version" || arg == L"-V") {
            showVersion = true;
            return true;
        }
        if (arg == L"--ssl") {
            useSsl = true;
            continue;
        }
        if (arg == L"--html") {
            isHtml = true;
            continue;
        }
        if (arg == L"-v") {
            verbose = true;
            continue;
        }
        if (arg == L"-n") {
            dryRun = true;
            continue;
        }

        if (arg == L"-s" || arg == L"-r" || arg == L"-R" || arg == L"-S" || arg == L"-u" || arg == L"-p" ||
            arg == L"-c" || arg == L"-b" || arg == L"-a" || arg == L"-m" || arg == L"-f" || arg == L"-H" || arg == L"-t") {
            if (i + 1 >= argc) {
                std::wcerr << L"mailx: option requires an argument -- " << arg << L"\n";
                return false;
            }

            std::wstring v = argv[++i] ? argv[i] : L"";
            if (arg == L"-s") {
                subject = v;
            } else if (arg == L"-r") {
                from = v;
            } else if (arg == L"-R") {
                replyTo = v;
            } else if (arg == L"-S") {
                size_t colon = v.find(L':');
                if (colon == std::wstring::npos) {
                    server = v;
                } else {
                    server = v.substr(0, colon);
                    port = _wtoi(v.substr(colon + 1).c_str());
                    if (port <= 0) {
                        std::wcerr << L"mailx: invalid SMTP port\n";
                        return false;
                    }
                }
            } else if (arg == L"-u") {
                user = v;
            } else if (arg == L"-p") {
                password = v;
            } else if (arg == L"-c") {
                cc.push_back(v);
            } else if (arg == L"-b") {
                bcc.push_back(v);
            } else if (arg == L"-a") {
                attachments.push_back(v);
            } else if (arg == L"-m") {
                body = v;
            } else if (arg == L"-f") {
                bodyFile = v;
            } else if (arg == L"-H") {
                size_t sep = v.find(L':');
                if (sep == std::wstring::npos) {
                    std::wcerr << L"mailx: invalid header format (expected NAME:VALUE)\n";
                    return false;
                }
                std::wstring name = Trim(v.substr(0, sep));
                std::wstring value = Trim(v.substr(sep + 1));
                if (name.empty()) {
                    std::wcerr << L"mailx: invalid header name\n";
                    return false;
                }
                headers.push_back({ name, value });
            } else if (arg == L"-t") {
                timeoutMs = _wtoi(v.c_str());
                if (timeoutMs <= 0) {
                    std::wcerr << L"mailx: timeout must be > 0\n";
                    return false;
                }
            }
            continue;
        }

        if (!arg.empty() && arg[0] == L'-') {
            std::wcerr << L"mailx: unrecognized option '" << arg << L"'\n";
            return false;
        }

        to.push_back(arg);
    }

    return true;
}

void MailOptions::PrintHelp() const {
    std::wcout << LR"(mailx(1)                CrossShell for UNIX Reference Manual                 mailx(1)

    NAME
        mailx - send and receive Internet mail via SMTP/Windows Mail APIs

    SYNOPSIS
        mailx [OPTIONS] [RECIPIENT...]

    DESCRIPTION
        mailx is an interactive and command-line email utility supporting SMTP
        relay, attachments, and message composition.

    OPTIONS
        -s, --subject SUBJECT
            Specify subject on command line.

        -a, --attach FILE
            Attach FILE to the message.

        -r, --from ADDRESS
            Specify sender address.

        -c, --cc ADDRESS
            Specify Carbon Copy recipient.

        -b, --bcc ADDRESS
            Specify Blind Carbon Copy recipient.

        --json, --csv, --table
            Format dispatch results as JSON, CSV, or table.

        --pipe COMMAND
            Send dispatch diagnostics through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        mailx -s "Build Report" -a dist.zip dev@example.com < notes.txt
            Send email with subject and attachment.

    CrossShell for UNIX                                                  mailx(1)
)";
}

void MailOptions::PrintVersion() const {
    std::wcout << L"mailx 2.0.0\n";
}
