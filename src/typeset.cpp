/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cwctype>
#include <algorithm>
#include <windows.h>
#include <cstdio>
#include <streambuf>
#include <sstream>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "User32.lib")

// ============================================================================
// 1. DATA MODELS & ENUMS
// ============================================================================

enum class OutputFormat {
    Default = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

struct VariableAttributes {
    bool isExport = false;
    bool isReadOnly = false;
    bool isUpper = false;
    bool isLower = false;
    bool isInteger = false;
    int integerBase = 10;
    bool isLeftJustify = false;
    size_t leftWidth = 0;
    bool isRightJustify = false;
    size_t rightWidth = 0;
    bool isZeroFill = false;
    size_t zeroWidth = 0;
    bool isTagged = false;
    bool isArray = false;
    bool isAssoc = false;
    bool isNameRef = false;
    bool isGlobal = false;
    bool isFunction = false;
};

// ============================================================================
// 2. STRING UTILITIES & FORMATTERS
// ============================================================================

class StringUtils {
public:
    static std::wstring ToUpper(std::wstring str) {
        for (auto& c : str) c = static_cast<wchar_t>(std::towupper(c));
        return str;
    }

    static std::wstring ToLower(std::wstring str) {
        for (auto& c : str) c = static_cast<wchar_t>(std::towlower(c));
        return str;
    }

    static std::wstring ToInteger(const std::wstring& str, int base = 10) {
        if (str.empty()) return L"0";
        try {
            size_t idx = 0;
            int b = (base >= 2 && base <= 36) ? base : 10;
            long long val = std::stoll(str, &idx, b);
            if (b == 10) {
                return std::to_wstring(val);
            } else {
                wchar_t buf[64];
                _i64tow_s(val, buf, 64, b);
                return ToUpper(std::wstring(buf));
            }
        } catch (...) {
            return str;
        }
    }

    static std::wstring LeftJustify(const std::wstring& str, size_t width) {
        if (width == 0) return str;
        if (str.length() > width) {
            return str.substr(0, width);
        }
        std::wstring res = str;
        res.append(width - str.length(), L' ');
        return res;
    }

    static std::wstring RightJustify(const std::wstring& str, size_t width) {
        if (width == 0) return str;
        if (str.length() > width) {
            return str.substr(str.length() - width);
        }
        return std::wstring(width - str.length(), L' ') + str;
    }

    static std::wstring ZeroFill(const std::wstring& str, size_t width) {
        if (width == 0) return str;
        size_t first = str.find_first_not_of(L' ');
        std::wstring trimmed = (first == std::wstring::npos) ? L"" : str.substr(first);
        if (trimmed.length() > width) {
            return trimmed.substr(trimmed.length() - width);
        }
        return std::wstring(width - trimmed.length(), L'0') + trimmed;
    }

    static std::wstring ApplyAttributes(const std::wstring& value, const VariableAttributes& attr) {
        std::wstring result = value;
        if (attr.isUpper) {
            result = ToUpper(result);
        } else if (attr.isLower) {
            result = ToLower(result);
        }

        if (attr.isInteger) {
            result = ToInteger(result, attr.integerBase);
        }

        if (attr.isZeroFill) {
            result = ZeroFill(result, attr.zeroWidth);
        } else if (attr.isLeftJustify) {
            result = LeftJustify(result, attr.leftWidth);
        } else if (attr.isRightJustify) {
            result = RightJustify(result, attr.rightWidth);
        }
        return result;
    }

    static std::wstring FormatFlags(const VariableAttributes& attr) {
        std::wstring flags;
        if (attr.isExport) flags += L" -x";
        if (attr.isReadOnly) flags += L" -r";
        if (attr.isInteger) {
            flags += L" -i";
            if (attr.integerBase != 10 && attr.integerBase > 0) flags += std::to_wstring(attr.integerBase);
        }
        if (attr.isUpper) flags += L" -u";
        if (attr.isLower) flags += L" -l";
        if (attr.isLeftJustify) flags += L" -L" + std::to_wstring(attr.leftWidth);
        if (attr.isRightJustify) flags += L" -R" + std::to_wstring(attr.rightWidth);
        if (attr.isZeroFill) flags += L" -Z" + std::to_wstring(attr.zeroWidth);
        if (attr.isTagged) flags += L" -t";
        if (attr.isArray) flags += L" -a";
        if (attr.isAssoc) flags += L" -A";
        if (attr.isNameRef) flags += L" -n";
        if (attr.isGlobal) flags += L" -g";
        if (flags.empty()) flags = L" -x";
        return flags;
    }
};

class OutputFormatter {
private:
    OutputFormat m_format{OutputFormat::Default};
    bool m_headerEmitted{false};

public:
    explicit OutputFormatter(OutputFormat fmt) : m_format(fmt) {}

    void EmitRecord(const std::wstring& name, const std::wstring& value, const VariableAttributes& attr = {}) {
        switch (m_format) {
            case OutputFormat::Json:
                std::wcout << L"{\"name\":\"" << name << L"\",\"value\":\"" << value << L"\"}\n";
                break;
            case OutputFormat::Csv:
                if (!m_headerEmitted) {
                    std::wcout << L"\"name\",\"value\"\n";
                    m_headerEmitted = true;
                }
                std::wcout << L"\"" << name << L"\",\"" << value << L"\"\n";
                break;
            case OutputFormat::Table:
                if (!m_headerEmitted) {
                    std::wcout << L"NAME\tVALUE\n";
                    m_headerEmitted = true;
                }
                std::wcout << name << L"\t" << value << L"\n";
                break;
            default:
                std::wcout << L"typeset" << StringUtils::FormatFlags(attr) << L" " << name << L"=\"" << value << L"\"\n";
                break;
        }
    }
};

class TypesetPipe : public std::wstreambuf {
private:
    FILE* m_file;
    wchar_t m_buffer[1024];

public:
    explicit TypesetPipe(FILE* f) : m_file(f) {
        setp(m_buffer, m_buffer + 1024);
    }

    int_type overflow(int_type c) override {
        if (c != traits_type::eof()) {
            *pptr() = static_cast<wchar_t>(c);
            pbump(1);
        }
        return sync() == 0 ? traits_type::not_eof(c) : traits_type::eof();
    }

    int sync() override {
        auto n = pptr() - pbase();
        if (n && m_file) {
            std::wstring value(pbase(), static_cast<size_t>(n));
            int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
            if (size > 0) {
                std::string utf8(static_cast<size_t>(size), '\0');
                WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), &utf8[0], size, nullptr, nullptr);
                fwrite(utf8.data(), 1, utf8.size(), m_file);
            }
        }
        setp(m_buffer, m_buffer + 1024);
        return m_file ? fflush(m_file) : 0;
    }
};

// ============================================================================
// 3. ENVIRONMENT STORES (PROCESS & REGISTRY)
// ============================================================================

class EnvironmentStore {
public:
    static std::map<std::wstring, std::wstring> GetProcessEnvironmentMap() {
        std::map<std::wstring, std::wstring> envMap;
        LPWCH envBlock = GetEnvironmentStringsW();
        if (!envBlock) return envMap;

        LPCWSTR p = envBlock;
        while (*p) {
            std::wstring entry(p);
            if (!entry.empty() && entry[0] != L'=') {
                size_t eqPos = entry.find(L'=');
                if (eqPos != std::wstring::npos) {
                    envMap[entry.substr(0, eqPos)] = entry.substr(eqPos + 1);
                }
            }
            p += wcslen(p) + 1;
        }
        FreeEnvironmentStringsW(envBlock);
        return envMap;
    }

    static bool SetProcessEnvironmentVariable(const std::wstring& var, const std::wstring& val) {
        return SetEnvironmentVariableW(var.c_str(), val.empty() ? nullptr : val.c_str()) != 0;
    }
};

class RegistryEnvironmentStore {
public:
    static bool SetPersistentUserEnv(const std::wstring& var, const std::wstring& val) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            if (val.empty()) {
                RegDeleteValueW(hKey, var.c_str());
            } else {
                RegSetValueExW(
                    hKey, 
                    var.c_str(), 
                    0, 
                    REG_SZ, 
                    reinterpret_cast<const BYTE*>(val.c_str()), 
                    static_cast<DWORD>((val.length() + 1) * sizeof(wchar_t))
                );
            }
            RegCloseKey(hKey);

            DWORD_PTR dwResult;
            SendMessageTimeoutW(
                HWND_BROADCAST, 
                WM_SETTINGCHANGE, 
                0, 
                reinterpret_cast<LPARAM>(L"Environment"), 
                SMTO_ABORTIFHUNG, 
                5000, 
                &dwResult
            );
            return true;
        }
        return false;
    }
};

// ============================================================================
// 4. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

struct TypesetOptions {
    VariableAttributes attributes;
    bool optPrint = false;
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat format = OutputFormat::Default;
    std::wstring pipeCommand;
    std::vector<std::wstring> targets;
};

class OptionParser {
public:
    static void PrintHelp() {
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

    static void PrintVersion() {
        std::wcout << L"typeset (CrossShell) 5.0.0\n";
    }

    bool Parse(int argc, wchar_t* argv[], TypesetOptions& opts) const {
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
};

class TypesetApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) {
        TypesetOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            OptionParser::PrintHelp();
            return 1;
        }

        if (opts.showHelp) {
            OptionParser::PrintHelp();
            return 0;
        }

        if (opts.showVersion) {
            OptionParser::PrintVersion();
            return 0;
        }

        auto envMap = EnvironmentStore::GetProcessEnvironmentMap();
        FILE* pipe = opts.pipeCommand.empty() ? nullptr : _wpopen(opts.pipeCommand.c_str(), L"w");
        std::wstreambuf* oldOutput = nullptr;
        TypesetPipe* pipeBuffer = nullptr;
        if (pipe) {
            oldOutput = std::wcout.rdbuf();
            pipeBuffer = new TypesetPipe(pipe);
            std::wcout.rdbuf(pipeBuffer);
        }

        OutputFormatter formatter(opts.format);

        // Case 1: No targets specified -> list all environment variables
        if (opts.targets.empty()) {
            for (const auto& pair : envMap) {
                VariableAttributes attr = opts.attributes;
                attr.isExport = true;
                formatter.EmitRecord(pair.first, pair.second, attr);
            }
            if (pipeBuffer) {
                std::wcout.flush();
                std::wcout.rdbuf(oldOutput);
                delete pipeBuffer;
                _pclose(pipe);
            }
            return 0;
        }

        // Case 2: Process targets (assignments or queries)
        for (const auto& target : opts.targets) {
            std::wstring varName;
            std::wstring varVal;
            size_t eqPos = target.find(L'=');
            bool hasAssignment = (eqPos != std::wstring::npos);

            if (hasAssignment) {
                varName = target.substr(0, eqPos);
                varVal = target.substr(eqPos + 1);
            } else {
                varName = target;
                if (envMap.find(varName) != envMap.end()) {
                    varVal = envMap[varName];
                }
            }

            // Apply formatting / value transformation qualifiers
            if (hasAssignment) {
                varVal = StringUtils::ApplyAttributes(varVal, opts.attributes);
            }

            // If print mode or query mode without assignment
            if (opts.optPrint || !hasAssignment) {
                VariableAttributes attr = opts.attributes;
                attr.isExport = true;
                formatter.EmitRecord(varName, varVal, attr);
                continue;
            }

            // Apply to process environment
            if (!varName.empty()) {
                EnvironmentStore::SetProcessEnvironmentVariable(varName, varVal);
                VariableAttributes attr = opts.attributes;
                attr.isExport = true;
                formatter.EmitRecord(varName, varVal, attr);
            }

            // Persist to Windows User Registry if -g / --global specified
            if (opts.attributes.isGlobal && !varName.empty()) {
                if (RegistryEnvironmentStore::SetPersistentUserEnv(varName, varVal)) {
                    if (opts.format == OutputFormat::Default) {
                        std::wcout << L"typeset: Saved '" << varName << L"' to Windows User Registry (HKCU\\Environment)\n";
                    }
                } else {
                    std::wcerr << L"typeset: Failed to save '" << varName << L"' to Windows Registry\n";
                }
            }
        }

        if (pipeBuffer) {
            std::wcout.flush();
            std::wcout.rdbuf(oldOutput);
            delete pipeBuffer;
            _pclose(pipe);
        }
        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    TypesetApplication app;
    return app.Run(argc, argv);
}