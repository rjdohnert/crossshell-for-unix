/*
BSD 3-Clause License

CrossShell for UNIX
Copyright (c) 2026, Roberto J Dohnert
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cwctype>
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>

class ExprParser {
public:
    explicit ExprParser(std::vector<std::wstring> tokens) : tokens_(std::move(tokens)) {}

    bool parse(std::wstring& out, std::wstring& err) {
        if (tokens_.empty()) {
            err = L"syntax error";
            return false;
        }
        pos_ = 0;
        out = parse_or(err);
        if (!err.empty()) return false;
        if (pos_ != tokens_.size()) {
            err = L"syntax error";
            return false;
        }
        return true;
    }

private:
    static bool is_int(const std::wstring& s) {
        if (s.empty()) return false;
        size_t i = (s[0] == L'-' || s[0] == L'+') ? 1 : 0;
        if (i == s.size()) return false;
        for (; i < s.size(); ++i) {
            if (!iswdigit(s[i])) return false;
        }
        return true;
    }

    static long long to_int(const std::wstring& s, bool& ok) {
        ok = false;
        try {
            size_t idx = 0;
            long long v = std::stoll(s, &idx, 10);
            if (idx != s.size()) return 0;
            ok = true;
            return v;
        } catch (...) {
            return 0;
        }
    }

    static bool truthy(const std::wstring& s) {
        return !(s.empty() || s == L"0");
    }

    static std::wstring as_num(long long v) {
        return std::to_wstring(v);
    }

    std::wstring parse_or(std::wstring& err) {
        std::wstring left = parse_and(err);
        while (err.empty() && match(L"|")) {
            std::wstring right = parse_and(err);
            if (!truthy(left)) left = right;
        }
        return left;
    }

    std::wstring parse_and(std::wstring& err) {
        std::wstring left = parse_cmp(err);
        while (err.empty() && match(L"&")) {
            std::wstring right = parse_cmp(err);
            if (!truthy(left) || !truthy(right)) left = L"0";
        }
        return left;
    }

    std::wstring parse_cmp(std::wstring& err) {
        std::wstring left = parse_add(err);
        while (err.empty() && has_more()) {
            std::wstring op = peek();
            if (op != L"=" && op != L"!=" && op != L"<" && op != L"<=" && op != L">" && op != L">=") {
                break;
            }
            next();
            std::wstring right = parse_add(err);
            if (!err.empty()) break;

            bool li = is_int(left), ri = is_int(right);
            bool result = false;
            if (li && ri) {
                bool okL = false, okR = false;
                long long lv = to_int(left, okL), rv = to_int(right, okR);
                if (!okL || !okR) {
                    err = L"non-integer argument";
                    return L"";
                }
                if (op == L"=") result = (lv == rv);
                else if (op == L"!=") result = (lv != rv);
                else if (op == L"<") result = (lv < rv);
                else if (op == L"<=") result = (lv <= rv);
                else if (op == L">") result = (lv > rv);
                else if (op == L">=") result = (lv >= rv);
            } else {
                if (op == L"=") result = (left == right);
                else if (op == L"!=") result = (left != right);
                else if (op == L"<") result = (left < right);
                else if (op == L"<=") result = (left <= right);
                else if (op == L">") result = (left > right);
                else if (op == L">=") result = (left >= right);
            }
            left = result ? L"1" : L"0";
        }
        return left;
    }

    std::wstring parse_add(std::wstring& err) {
        std::wstring left = parse_mul(err);
        while (err.empty() && has_more()) {
            std::wstring op = peek();
            if (op != L"+" && op != L"-") break;
            next();
            std::wstring right = parse_mul(err);
            if (!err.empty()) break;

            bool okL = false, okR = false;
            long long lv = to_int(left, okL), rv = to_int(right, okR);
            if (!okL || !okR) {
                err = L"non-integer argument";
                return L"";
            }
            left = as_num(op == L"+" ? lv + rv : lv - rv);
        }
        return left;
    }

    std::wstring parse_mul(std::wstring& err) {
        std::wstring left = parse_primary(err);
        while (err.empty() && has_more()) {
            std::wstring op = peek();
            if (op != L"*" && op != L"/" && op != L"%") break;
            next();
            std::wstring right = parse_primary(err);
            if (!err.empty()) break;

            bool okL = false, okR = false;
            long long lv = to_int(left, okL), rv = to_int(right, okR);
            if (!okL || !okR) {
                err = L"non-integer argument";
                return L"";
            }
            if ((op == L"/" || op == L"%") && rv == 0) {
                err = L"division by zero";
                return L"";
            }
            if (op == L"*") left = as_num(lv * rv);
            else if (op == L"/") left = as_num(lv / rv);
            else left = as_num(lv % rv);
        }
        return left;
    }

    std::wstring parse_primary(std::wstring& err) {
        if (!has_more()) {
            err = L"syntax error";
            return L"";
        }

        if (match(L"(")) {
            std::wstring v = parse_or(err);
            if (!err.empty()) return L"";
            if (!match(L")")) {
                err = L"missing ')'";
                return L"";
            }
            return v;
        }

        return next();
    }

    bool has_more() const { return pos_ < tokens_.size(); }
    std::wstring peek() const { return tokens_[pos_]; }
    std::wstring next() { return tokens_[pos_++]; }
    bool match(const std::wstring& tok) {
        if (has_more() && tokens_[pos_] == tok) {
            ++pos_;
            return true;
        }
        return false;
    }

    std::vector<std::wstring> tokens_;
    size_t pos_ = 0;
};

static void print_help() {
    std::wcout
        << L"Usage: expr EXPRESSION\n"
        << L"Evaluate EXPRESSION and print the result.\n\n"
        << L"Supported operators:\n"
        << L"  |  &  =  !=  <  <=  >  >=  +  -  *  /  %\n"
        << L"Parentheses are supported.\n\n"
        << L"Exit status is 0 if result is neither null nor 0; otherwise 1.\n"
        << L"  --help      display this help and exit\n"
        << L"  --version   output version information and exit\n"
        << L"  --json, --csv, --table  format the expression result\n"
        << L"  --pipe COMMAND          send the result through COMMAND\n";
}

static void print_version() {
    std::wcout << L"expr (CrossShellUX) 1.0.0\n";
}

static std::string to_utf8(const std::wstring& text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &result[0], size, nullptr, nullptr);
    return result;
}

static int expr_main(int argc, wchar_t* argv[]) {
    if (argc <= 1) {
        std::wcerr << L"expr: missing expression\n";
        return 2;
    }

    if (argc == 2) {
        std::wstring arg = argv[1] ? argv[1] : L"";
        if (arg == L"--help") {
            print_help();
            return 0;
        }
        if (arg == L"--version") {
            print_version();
            return 0;
        }
    }

    std::vector<std::wstring> tokens;
    int format = 0;
    std::wstring pipeCommand;
    for (int i = 1; i < argc; ++i) {
        std::wstring token = argv[i] ? argv[i] : L"";
        if (token == L"--json") format = 1;
        else if (token == L"--csv") format = 2;
        else if (token == L"--table") format = 3;
        else if (token == L"--pipe" && i + 1 < argc) pipeCommand = argv[++i];
        else tokens.push_back(token);
    }

    ExprParser parser(std::move(tokens));
    std::wstring result;
    std::wstring err;
    if (!parser.parse(result, err)) {
        std::wcerr << L"expr: " << err << L"\n";
        return 2;
    }

    std::wstring text = format == 1 ? L"{\"result\":\"" + result + L"\"}\n" : format == 2 ? L"result\n" + result + L"\n" : L"RESULT\n------\n" + result + L"\n";
    if (format || !pipeCommand.empty()) { if (!pipeCommand.empty()) { FILE* pipe = _wpopen(pipeCommand.c_str(), L"w"); if (!pipe) return 2; std::string narrow = to_utf8(text); fwrite(narrow.data(), 1, narrow.size(), pipe); _pclose(pipe); } else std::wcout << text; } else std::wcout << result << L"\n";
    return (result.empty() || result == L"0") ? 1 : 0;
}

class ExprApplication { public: int run(int argc, wchar_t* argv[]) const { return expr_main(argc, argv); } };
int wmain(int argc, wchar_t* argv[]) { return ExprApplication().run(argc, argv); }
