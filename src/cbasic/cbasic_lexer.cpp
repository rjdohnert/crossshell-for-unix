#include "cbasic_lexer.hpp"
#include <algorithm>
#include <cctype>
#include <utility>

class Lexer {
    std::string source;
    size_t start = 0;
    size_t current = 0;
    int line = 1;

    bool isAtEnd() const { return current >= source.length(); }
    char advance() { return source[current++]; }
    
    char peek(size_t offset = 0) const { 
        if (current + offset >= source.length()) return '\0';
        return source[current + offset]; 
    }

    Token makeToken(SyntaxTokenType type, Value val = 0.0, std::string customLexeme = "") {
        std::string rawLex = source.substr(start, current - start);
        std::string lex = customLexeme.empty() ? rawLex : customLexeme;
        return {type, lex, rawLex, val, line};
    }

public:
    Lexer(std::string src) : source(std::move(src)) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (!isAtEnd()) {
            start = current;
            char c = advance();

            if (c == ' ' || c == '\r' || c == '\t') continue;
            if (c == '\n') { line++; tokens.push_back(makeToken(SyntaxTokenType::NEWLINE)); continue; }
            if (c == ':') { tokens.push_back(makeToken(SyntaxTokenType::COLON)); continue; }
            if (c == ';') { tokens.push_back(makeToken(SyntaxTokenType::SEMICOLON)); continue; }
            if (c == '+') { tokens.push_back(makeToken(SyntaxTokenType::PLUS)); continue; }
            if (c == '-') { tokens.push_back(makeToken(SyntaxTokenType::MINUS)); continue; }
            if (c == '*') { tokens.push_back(makeToken(SyntaxTokenType::STAR)); continue; }
            if (c == '^') { tokens.push_back(makeToken(SyntaxTokenType::CARET)); continue; }
            if (c == '/') { tokens.push_back(makeToken(SyntaxTokenType::SLASH)); continue; }
            if (c == '\\') { tokens.push_back(makeToken(SyntaxTokenType::BACKSLASH)); continue; }
            if (c == '=') { tokens.push_back(makeToken(SyntaxTokenType::EQUAL)); continue; }
            
            if (c == '<') {
                if (peek() == '=') { advance(); tokens.push_back(makeToken(SyntaxTokenType::LESS_EQUAL)); }
                else if (peek() == '>') { advance(); tokens.push_back(makeToken(SyntaxTokenType::NOT_EQUAL)); }
                else { tokens.push_back(makeToken(SyntaxTokenType::LESS)); }
                continue;
            }
            if (c == '>') {
                if (peek() == '=') { advance(); tokens.push_back(makeToken(SyntaxTokenType::GREATER_EQUAL)); }
                else { tokens.push_back(makeToken(SyntaxTokenType::GREATER)); }
                continue;
            }
            if (c == '!') {
                if (peek() == '=') { advance(); tokens.push_back(makeToken(SyntaxTokenType::NOT_EQUAL)); continue; }
            }

            if (c == '(') { tokens.push_back(makeToken(SyntaxTokenType::LEFT_PAREN)); continue; }
            if (c == ')') { tokens.push_back(makeToken(SyntaxTokenType::RIGHT_PAREN)); continue; }
            if (c == ',') { tokens.push_back(makeToken(SyntaxTokenType::COMMA)); continue; }
            
            if (c == '\'') { // Single line comment
                while (peek() != '\n' && !isAtEnd()) advance();
                continue;
            }

            if (std::isdigit(c) || (c == '.' && std::isdigit(peek()))) {
                while (std::isdigit(peek())) advance();
                if (peek() == '.' && std::isdigit(peek(1))) {
                    advance();
                    while (std::isdigit(peek())) advance();
                }
                double val = std::stod(source.substr(start, current - start));
                tokens.push_back(makeToken(SyntaxTokenType::NUMBER, val));
                continue;
            }

            if (c == '"') {
                std::string str;
                while (peek() != '"' && !isAtEnd()) {
                    if (peek() == '\n') line++;
                    str += advance();
                }
                if (!isAtEnd()) advance();
                tokens.push_back(makeToken(SyntaxTokenType::STRING, str));
                continue;
            }

            if (std::isalpha(c) || c == '_') {
                while (std::isalnum(peek()) || peek() == '_' || peek() == '$' || peek() == '#') advance();
                std::string text = source.substr(start, current - start);
                std::string upperText = text;
                std::transform(upperText.begin(), upperText.end(), upperText.begin(), ::toupper);

                if (upperText == "PRINT") tokens.push_back(makeToken(SyntaxTokenType::PRINT));
                else if (upperText == "PRINT#") tokens.push_back(makeToken(SyntaxTokenType::PRINT_FILE));
                else if (upperText == "INPUT") tokens.push_back(makeToken(SyntaxTokenType::INPUT));
                else if (upperText == "INPUT#") tokens.push_back(makeToken(SyntaxTokenType::INPUT_FILE));
                else if (upperText == "LET") tokens.push_back(makeToken(SyntaxTokenType::LET));
                else if (upperText == "IF") tokens.push_back(makeToken(SyntaxTokenType::IF));
                else if (upperText == "THEN") tokens.push_back(makeToken(SyntaxTokenType::THEN));
                else if (upperText == "ELSE") tokens.push_back(makeToken(SyntaxTokenType::ELSE));
                else if (upperText == "GOTO") tokens.push_back(makeToken(SyntaxTokenType::GOTO));
                else if (upperText == "GOSUB") tokens.push_back(makeToken(SyntaxTokenType::GOSUB));
                else if (upperText == "RETURN") tokens.push_back(makeToken(SyntaxTokenType::RETURN));
                else if (upperText == "ON") tokens.push_back(makeToken(SyntaxTokenType::ON));
                else if (upperText == "RESUME") tokens.push_back(makeToken(SyntaxTokenType::RESUME));
                else if (upperText == "FOR") tokens.push_back(makeToken(SyntaxTokenType::FOR));
                else if (upperText == "TO") tokens.push_back(makeToken(SyntaxTokenType::TO));
                else if (upperText == "STEP") tokens.push_back(makeToken(SyntaxTokenType::STEP));
                else if (upperText == "NEXT") tokens.push_back(makeToken(SyntaxTokenType::NEXT));
                else if (upperText == "DIM") tokens.push_back(makeToken(SyntaxTokenType::DIM));
                else if (upperText == "DATA") tokens.push_back(makeToken(SyntaxTokenType::DATA));
                else if (upperText == "READ") tokens.push_back(makeToken(SyntaxTokenType::READ));
                else if (upperText == "RESTORE") tokens.push_back(makeToken(SyntaxTokenType::RESTORE));
                else if (upperText == "DEF") tokens.push_back(makeToken(SyntaxTokenType::DEF));
                else if (upperText == "OPTION") tokens.push_back(makeToken(SyntaxTokenType::OPTION));
                else if (upperText == "BASE") tokens.push_back(makeToken(SyntaxTokenType::BASE));
                else if (upperText == "ERASE") tokens.push_back(makeToken(SyntaxTokenType::ERASE));
                else if (upperText == "OPEN") tokens.push_back(makeToken(SyntaxTokenType::OPEN));
                else if (upperText == "CLOSE") tokens.push_back(makeToken(SyntaxTokenType::CLOSE));
                else if (upperText == "CLS") tokens.push_back(makeToken(SyntaxTokenType::CLS));
                else if (upperText == "LOCATE") tokens.push_back(makeToken(SyntaxTokenType::LOCATE));
                else if (upperText == "COLOR") tokens.push_back(makeToken(SyntaxTokenType::COLOR));
                else if (upperText == "SCREEN") tokens.push_back(makeToken(SyntaxTokenType::SCREEN));
                else if (upperText == "LINE") tokens.push_back(makeToken(SyntaxTokenType::LINE));
                else if (upperText == "PSET") tokens.push_back(makeToken(SyntaxTokenType::PSET));
                else if (upperText == "PLAY") tokens.push_back(makeToken(SyntaxTokenType::PLAY));
                else if (upperText == "SOUND") tokens.push_back(makeToken(SyntaxTokenType::SOUND));
                else if (upperText == "FIELD") tokens.push_back(makeToken(SyntaxTokenType::FIELD));
                else if (upperText == "GET") tokens.push_back(makeToken(SyntaxTokenType::GET));
                else if (upperText == "PUT") tokens.push_back(makeToken(SyntaxTokenType::PUT));
                else if (upperText == "MOD") tokens.push_back(makeToken(SyntaxTokenType::MOD));
                else if (upperText == "AND") tokens.push_back(makeToken(SyntaxTokenType::AND));
                else if (upperText == "OR") tokens.push_back(makeToken(SyntaxTokenType::OR));
                else if (upperText == "NOT") tokens.push_back(makeToken(SyntaxTokenType::NOT));
                else if (upperText == "HELP") tokens.push_back(makeToken(SyntaxTokenType::HELP));
                else if (upperText == "VERSION") tokens.push_back(makeToken(SyntaxTokenType::VERSION));
                else if (upperText == "SAVE") tokens.push_back(makeToken(SyntaxTokenType::SAVE));
                else if (upperText == "LOAD") tokens.push_back(makeToken(SyntaxTokenType::LOAD));
                else if (upperText == "LIST") tokens.push_back(makeToken(SyntaxTokenType::LIST));
                else if (upperText == "RUN") tokens.push_back(makeToken(SyntaxTokenType::RUN));
                else if (upperText == "NEW") tokens.push_back(makeToken(SyntaxTokenType::NEW));
                else if (upperText == "POKE") tokens.push_back(makeToken(SyntaxTokenType::POKE));
                else if (upperText == "END") tokens.push_back(makeToken(SyntaxTokenType::END));
                else if (upperText == "REM") {
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    tokens.push_back(makeToken(SyntaxTokenType::IDENTIFIER, upperText, upperText));
                }
                continue;
            }
        }
        tokens.push_back({SyntaxTokenType::END_OF_FILE, "", "", 0.0, line});
        return tokens;
    }
};


std::vector<Token> tokenizeSource(std::string source) {
    return Lexer(std::move(source)).tokenize();
}
