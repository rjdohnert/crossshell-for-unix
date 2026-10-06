#pragma once

#include "typeset.hpp"
#include "variable_attributes.hpp"

class StringUtils {
public:
    static std::wstring ToUpper(std::wstring str);

    static std::wstring ToLower(std::wstring str);

    static std::wstring ToInteger(const std::wstring& str, int base = 10);

    static std::wstring LeftJustify(const std::wstring& str, size_t width);

    static std::wstring RightJustify(const std::wstring& str, size_t width);

    static std::wstring ZeroFill(const std::wstring& str, size_t width);

    static std::wstring ApplyAttributes(const std::wstring& value, const VariableAttributes& attr);

    static std::wstring FormatFlags(const VariableAttributes& attr);
};
