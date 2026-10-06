/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * nl - number lines of files
 */

#ifndef NL_HPP
#define NL_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <sstream>
#include <iomanip>
#include <memory>
#include <cctype>

enum class StyleMode { ALL, NON_EMPTY, NONE, REGEX };
enum class NumberFormat { LN, RN, RZ }; // Left, Right, Right Zero-Padded
enum class SectionType { HEADER, BODY, FOOTER };

struct NlStyle {
    StyleMode mode{StyleMode::NON_EMPTY};
    std::regex pattern;
    std::string rawPattern;

    static NlStyle parse(const std::string& arg);
};

#endif // NL_HPP
