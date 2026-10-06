#pragma once

#include "typeset.hpp"

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
