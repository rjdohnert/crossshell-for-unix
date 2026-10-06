#ifndef PKG_HPP
#define PKG_HPP

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <cwctype>
#include <memory>

class ArgumentEscaper {
public:
    static std::wstring escape(const std::wstring& arg);
    static std::wstring toLower(std::wstring value);
    static bool endsWithCi(const std::wstring& value, const std::wstring& suffix);
};

#endif // PKG_HPP
