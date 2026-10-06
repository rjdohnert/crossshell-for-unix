#include "evaluator.hpp"
#include "file_system_inspector.hpp"
#include "type_parser.hpp"

Evaluator::Evaluator(const std::vector<std::wstring>& t) : tokens(t), idx(0) {}

bool Evaluator::Parse() {
        if (tokens.empty()) return false;
        bool res = ParseOr();
        if (idx < tokens.size()) {
            throw std::runtime_error("too many arguments");
        }
        return res;
    }

bool Evaluator::ParseOr() {
        bool left = ParseAnd();
        while (idx < tokens.size() && tokens[idx] == L"-o") {
            idx++;
            bool right = ParseAnd();
            left = left || right;
        }
        return left;
    }

bool Evaluator::ParseAnd() {
        bool left = ParseNot();
        while (idx < tokens.size() && tokens[idx] == L"-a") {
            idx++;
            bool right = ParseNot();
            left = left && right;
        }
        return left;
    }

bool Evaluator::ParseNot() {
        if (idx < tokens.size() && tokens[idx] == L"!") {
            idx++;
            return !ParseNot();
        }
        return ParseFactor();
    }

bool Evaluator::ParseFactor() {
        if (idx >= tokens.size()) {
            throw std::runtime_error("argument expected");
        }

        if (tokens[idx] == L"(") {
            idx++;
            bool val = ParseOr();
            if (idx >= tokens.size() || tokens[idx] != L")") {
                throw std::runtime_error("missing ')'");
            }
            idx++;
            return val;
        }

        return ParsePrimary();
    }

bool Evaluator::ParsePrimary() {
        if (idx >= tokens.size()) return false;

        if (idx + 2 < tokens.size() && IsBinaryOp(tokens[idx + 1])) {
            std::wstring left = tokens[idx];
            std::wstring op = tokens[idx + 1];
            std::wstring right = tokens[idx + 2];
            idx += 3;
            return EvalBinary(op, left, right);
        }

        if (IsUnaryOp(tokens[idx])) {
            std::wstring op = tokens[idx];
            idx++;
            std::wstring arg = L"";
            if (idx < tokens.size()) {
                arg = tokens[idx];
                idx++;
            }
            return EvalUnary(op, arg);
        }

        std::wstring str = tokens[idx];
        idx++;
        return !str.empty();
    }

bool Evaluator::IsUnaryOp(const std::wstring& op) {
        static const std::vector<std::wstring> ops = {
            L"-b", L"-c", L"-d", L"-e", L"-a", L"-f", L"-g", L"-h", L"-L", L"-k",
            L"-p", L"-r", L"-s", L"-S", L"-t", L"-u", L"-w", L"-x", L"-O", L"-G",
            L"-z", L"-n"
        };
        return std::find(ops.begin(), ops.end(), op) != ops.end();
    }

bool Evaluator::IsBinaryOp(const std::wstring& op) {
        static const std::vector<std::wstring> ops = {
            L"=", L"==", L"!=", L"<", L">",
            L"-eq", L"-ne", L"-gt", L"-ge", L"-lt", L"-le",
            L"-nt", L"-ot", L"-ef"
        };
        return std::find(ops.begin(), ops.end(), op) != ops.end();
    }

bool Evaluator::EvalUnary(const std::wstring& op, const std::wstring& arg) {
        if (op == L"-z") return arg.empty();
        if (op == L"-n") return !arg.empty();
        if (op == L"-e" || op == L"-a") return FileSystemInspector::FileExists(arg);
        if (op == L"-d") return FileSystemInspector::IsDirectory(arg);
        if (op == L"-f") return FileSystemInspector::IsRegularFile(arg);
        if (op == L"-s") return FileSystemInspector::IsNonEmpty(arg);
        if (op == L"-h" || op == L"-L") return FileSystemInspector::IsSymlink(arg);
        if (op == L"-r") return FileSystemInspector::IsReadable(arg);
        if (op == L"-w") return FileSystemInspector::IsWritable(arg);
        if (op == L"-x") return FileSystemInspector::IsExecutable(arg);
        if (op == L"-t") return FileSystemInspector::IsTerminal(arg);
        if (op == L"-c") return FileSystemInspector::IsCharDevice(arg);
        if (op == L"-p" || op == L"-S") return FileSystemInspector::IsNamedPipe(arg);
        if (op == L"-b") return false;
        if (op == L"-g" || op == L"-u" || op == L"-k" || op == L"-O" || op == L"-G") {
            return FileSystemInspector::FileExists(arg);
        }
        return false;
    }

bool Evaluator::EvalBinary(const std::wstring& op, const std::wstring& left, const std::wstring& right) {
        if (op == L"=" || op == L"==") return left == right;
        if (op == L"!=") return left != right;
        if (op == L"<") return left < right;
        if (op == L">") return left > right;

        if (op == L"-nt") return FileSystemInspector::FileNewerThan(left, right);
        if (op == L"-ot") return FileSystemInspector::FileOlderThan(left, right);
        if (op == L"-ef") return FileSystemInspector::SameFile(left, right);

        long long n1 = TypeParser::ParseLong(left);
        long long n2 = TypeParser::ParseLong(right);

        if (op == L"-eq") return n1 == n2;
        if (op == L"-ne") return n1 != n2;
        if (op == L"-gt") return n1 > n2;
        if (op == L"-ge") return n1 >= n2;
        if (op == L"-lt") return n1 < n2;
        if (op == L"-le") return n1 <= n2;

        return false;
    }
