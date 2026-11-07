#pragma once

#include <cstdint>

enum class Lang {
    Plain, C_Cpp, Java, Python, Swift, Kotlin, Fortran,
    Cobol, Pascal, JavaScript, Basic, PowerShell, Batch, Shell
};

enum class EveTheme { Default, Black, Orange, Green, Red, Blue, Pink };

enum class SyntaxTokenType : uint8_t {
    Default, Keyword, Type, String, Comment, Number, Directive
};

struct HighlightState {
    char commentTerminator = '\0';
    char quote = '\0';

    bool operator==(const HighlightState& other) const {
        return commentTerminator == other.commentTerminator && quote == other.quote;
    }
};
