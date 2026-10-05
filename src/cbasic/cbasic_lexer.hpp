#pragma once
#include "cbasic_common.hpp"
#include <vector>

// ============================================================================
// 2. LEXER (TOKENIZER)
// ============================================================================
enum class SyntaxTokenType {
    NUMBER, STRING, IDENTIFIER,
    PRINT, INPUT, LET, IF, THEN, ELSE, GOTO, GOSUB, RETURN, ON, RESUME,
    FOR, TO, STEP, NEXT, DIM, DATA, READ, RESTORE,
    DEF, OPTION, BASE, ERASE,
    OPEN, CLOSE, PRINT_FILE, INPUT_FILE,
    CLS, LOCATE, COLOR, SCREEN, LINE, PSET, PLAY, SOUND,
    FIELD, GET, PUT,
    END, REM, HELP, VERSION, SAVE, LOAD, LIST, RUN, NEW, POKE,
    PLUS, MINUS, STAR, SLASH, BACKSLASH, MOD, EQUAL, LESS, GREATER,
    CARET,
    LESS_EQUAL, GREATER_EQUAL, NOT_EQUAL,
    AND, OR, NOT,
    LEFT_PAREN, RIGHT_PAREN, COMMA, SEMICOLON,
    COLON, NEWLINE, END_OF_FILE
};

struct Token {
    SyntaxTokenType type;
    std::string lexeme;
    std::string rawLexeme;
    Value value;
    int line;
};

std::vector<Token> tokenizeSource(std::string source);
