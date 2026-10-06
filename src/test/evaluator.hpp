#pragma once

#include "test.hpp"

class Evaluator {
    std::vector<std::wstring> tokens;
    size_t idx = 0;

public:
    explicit Evaluator(const std::vector<std::wstring>& t);

    bool Parse();

private:
    bool ParseOr();

    bool ParseAnd();

    bool ParseNot();

    bool ParseFactor();

    bool ParsePrimary();

    static bool IsUnaryOp(const std::wstring& op);

    static bool IsBinaryOp(const std::wstring& op);

    static bool EvalUnary(const std::wstring& op, const std::wstring& arg);

    static bool EvalBinary(const std::wstring& op, const std::wstring& left, const std::wstring& right);
};
