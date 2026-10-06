#include "string_utils.hpp"
#include "variable_attributes.hpp"

std::wstring StringUtils::ToUpper(std::wstring str) {
        for (auto& c : str) c = static_cast<wchar_t>(std::towupper(c));
        return str;
    }

std::wstring StringUtils::ToLower(std::wstring str) {
        for (auto& c : str) c = static_cast<wchar_t>(std::towlower(c));
        return str;
    }

std::wstring StringUtils::ToInteger(const std::wstring& str, int base ) {
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

std::wstring StringUtils::LeftJustify(const std::wstring& str, size_t width) {
        if (width == 0) return str;
        if (str.length() > width) {
            return str.substr(0, width);
        }
        std::wstring res = str;
        res.append(width - str.length(), L' ');
        return res;
    }

std::wstring StringUtils::RightJustify(const std::wstring& str, size_t width) {
        if (width == 0) return str;
        if (str.length() > width) {
            return str.substr(str.length() - width);
        }
        return std::wstring(width - str.length(), L' ') + str;
    }

std::wstring StringUtils::ZeroFill(const std::wstring& str, size_t width) {
        if (width == 0) return str;
        size_t first = str.find_first_not_of(L' ');
        std::wstring trimmed = (first == std::wstring::npos) ? L"" : str.substr(first);
        if (trimmed.length() > width) {
            return trimmed.substr(trimmed.length() - width);
        }
        return std::wstring(width - trimmed.length(), L'0') + trimmed;
    }

std::wstring StringUtils::ApplyAttributes(const std::wstring& value, const VariableAttributes& attr) {
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

std::wstring StringUtils::FormatFlags(const VariableAttributes& attr) {
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
