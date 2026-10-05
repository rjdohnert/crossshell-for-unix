/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
Standardized Section Index
----------------------------
1. Core value types, sandbox path helpers, and utility conversions
2. Lexer/parser, AST nodes, and compile-time statement handling
3. Bytecode instruction model, compiler passes, and symbol management
4. VM runtime, execution loop, control flow, and error trapping
5. Built-in functions, file I/O, arrays, and graphics/compatibility shims
6. CLI/REPL entry flow, script loading, and top-level command dispatch
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <variant>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <limits>
#include <filesystem>
#include <system_error>
#include <functional>
#include "cbasic_common.hpp"
#include "cbasic_lexer.hpp"

// ============================================================================
// 1. DATA TYPES & HELPERS
// ============================================================================
// ============================================================================
// 3. BYTECODE COMPILER
// ============================================================================
enum class OpCode {
    PUSH_VAL, LOAD_VAR, STORE_VAR,
    DIM_ARRAY, LOAD_ARRAY, STORE_ARRAY,
    SET_OPTION_BASE, ERASE_ARRAY,
    FILE_OPEN, FILE_CLOSE, FILE_CLOSE_ALL, FILE_PRINT, FILE_INPUT,
    FILE_RANDOM_FIELD, FILE_RANDOM_GET, FILE_RANDOM_PUT,
    SET_ON_ERROR, RESUME_ERROR,
    CMD_CLS, CMD_LOCATE, CMD_COLOR, CMD_SCREEN, CMD_LINE, CMD_PSET, CMD_PLAY, CMD_SOUND,
    ADD, SUB, MUL, DIV, INT_DIV, MODULO, POW, NEGATE,
    CMP_EQ, CMP_LT, CMP_GT, CMP_LE, CMP_GE, CMP_NE,
    LOGICAL_AND, LOGICAL_OR, LOGICAL_NOT,
    PRINT_VAL, PRINT_TAB, PRINT_TAB_STOP, PRINT_NEWLINE, INPUT,
    JUMP, JUMP_IF_FALSE, ON_GOTO, ON_GOSUB, CALL, RETURN,
    FOR_INIT, FOR_NEXT,
    READ_VAR, RESTORE_DATA, POKE_MEM,
    FN_SQRT, FN_ABS, FN_RND, FN_INT, FN_LEN, FN_POS, FN_PEEK, FN_USR, FN_INKEY, FN_ERR, FN_ERL, FN_EOF,
    FN_LEFT, FN_RIGHT, FN_MID, FN_CHR, FN_ASC, FN_VAL, FN_STR,
    SHOW_HELP, SHOW_VERSION, HALT
};

struct Instruction {
    OpCode op;
    Value operand;
    int targetIp = -1;
    int line = 0;
    std::vector<int> targets;
};

struct CompileTimeForLoop {
    std::string varName;
    size_t loopStartIp;
    size_t forInitIp;
};

struct PendingOnJump {
    size_t instructionIndex;
    size_t tableIndex;
    int targetLine;
};

struct UserFunctionDefinition {
    std::string name;
    std::string paramName;
    int paramSymbolIndex = -1;
    std::vector<Token> bodyTokens;
    int definitionLine = 0;
};

class Compiler {
    std::vector<Token> tokens;
    size_t current = 0;
    int currentSourceLine = 1;
    
    std::vector<Instruction> code;
    std::vector<Value> dataValues;

    std::vector<std::string> symbolTable;
    std::unordered_map<std::string, int> symbolMap;

    std::map<int, size_t> lineNumberToIpMap;
    std::vector<std::pair<size_t, int>> pendingJumps;
    std::vector<PendingOnJump> pendingOnJumps;
    std::vector<CompileTimeForLoop> forStack;
    std::unordered_map<std::string, UserFunctionDefinition> userFunctions;
    std::vector<std::string> compileErrors;
    int expressionDepth = 0;
    int powerDepth = 0;
    static constexpr int kMaxExpressionDepth = 1024;
    static constexpr int kMaxPowerDepth = 2048;

    bool isAtEnd() const { return tokens[current].type == SyntaxTokenType::END_OF_FILE; }
    Token peek(size_t offset = 0) const { 
        if (current + offset >= tokens.size()) return tokens.back();
        return tokens[current + offset]; 
    }
    Token advance() { 
        Token t = tokens[current++];
        if (t.line > 0) currentSourceLine = t.line;
        return t;
    }
    
    bool check(SyntaxTokenType type) const { return peek().type == type; }
    bool match(SyntaxTokenType type) {
        if (check(type)) { advance(); return true; }
        return false;
    }

    bool checkIdentifierLexeme(const std::string& lexeme) const {
        return check(SyntaxTokenType::IDENTIFIER) && peek().lexeme == lexeme;
    }

    bool matchIdentifierLexeme(const std::string& lexeme) {
        if (checkIdentifierLexeme(lexeme)) {
            advance();
            return true;
        }
        return false;
    }

    int getSymbolIndex(const std::string& name) {
        if (symbolMap.count(name)) return symbolMap[name];
        int idx = static_cast<int>(symbolTable.size());
        symbolMap[name] = idx;
        symbolTable.push_back(name);
        return idx;
    }

    void emit(OpCode op, Value operand = 0.0, int targetIp = -1) {
        code.push_back({op, operand, targetIp, currentSourceLine, {}});
    }

    void emit(OpCode op, int operand, int targetIp = -1) {
        code.push_back({op, Value(static_cast<double>(operand)), targetIp, currentSourceLine, {}});
    }

    void reportCompileError(const std::string& message, int line = -1) {
        if (line <= 0) line = currentSourceLine;
        compileErrors.push_back("Compilation Error (line " + std::to_string(line) + "): " + message);
    }

    void emitBinaryOp(OpCode op) {
        if (code.size() >= 2 &&
            code.back().op == OpCode::PUSH_VAL &&
            code[code.size() - 2].op == OpCode::PUSH_VAL &&
            std::holds_alternative<double>(code.back().operand) &&
            std::holds_alternative<double>(code[code.size() - 2].operand)) {
            
            double b = std::get<double>(code.back().operand);
            code.pop_back();
            double a = std::get<double>(code.back().operand);
            code.pop_back();

            double res = 0.0;
            bool folded = true;

            switch (op) {
                case OpCode::ADD: res = a + b; break;
                case OpCode::SUB: res = a - b; break;
                case OpCode::MUL: res = a * b; break;
                case OpCode::DIV: res = (b != 0.0) ? a / b : 0.0; break;
                case OpCode::INT_DIV: res = (b != 0.0) ? std::floor(a / b) : 0.0; break;
                case OpCode::MODULO: res = (b != 0.0) ? std::fmod(a, b) : 0.0; break;
                case OpCode::POW: res = std::pow(a, b); break;
                default: folded = false; break;
            }

            if (folded) {
                emit(OpCode::PUSH_VAL, res);
                return;
            }
        }
        emit(op);
    }

    bool enterExpressionDepth() {
        if (expressionDepth >= kMaxExpressionDepth) {
            reportCompileError("Expression nesting too deep.");
            return false;
        }
        ++expressionDepth;
        return true;
    }

    void leaveExpressionDepth() {
        if (expressionDepth > 0) --expressionDepth;
    }

    bool enterPowerDepth() {
        if (powerDepth >= kMaxPowerDepth) {
            reportCompileError("Exponent chain is too deep.");
            return false;
        }
        ++powerDepth;
        return true;
    }

    void leavePowerDepth() {
        if (powerDepth > 0) --powerDepth;
    }

    bool parseArraySubscripts(int& dims) {
        if (!match(SyntaxTokenType::LEFT_PAREN)) return false;
        dims = 0;
        do {
            parseExpression();
            dims++;
        } while (match(SyntaxTokenType::COMMA));
        if (!match(SyntaxTokenType::RIGHT_PAREN)) {
            reportCompileError("Expected ')' to close array subscripts.");
        }
        return true;
    }

    bool emitUserFunctionCall(const std::string& id) {
        auto fnIt = userFunctions.find(id);
        if (fnIt == userFunctions.end()) return false;
        if (!check(SyntaxTokenType::LEFT_PAREN)) {
            reportCompileError("Function " + id + " requires parentheses and one argument.");
            emit(OpCode::PUSH_VAL, 0.0);
            return true;
        }

        advance();
        parseExpression();
        if (!match(SyntaxTokenType::RIGHT_PAREN)) {
            reportCompileError("Function " + id + " call is missing closing ')'.");
        }

        const auto& def = fnIt->second;
        emit(OpCode::STORE_VAR, def.paramSymbolIndex);

        auto savedTokens = tokens;
        size_t savedCurrent = current;
        int savedLine = currentSourceLine;

        tokens = def.bodyTokens;
        tokens.push_back({SyntaxTokenType::END_OF_FILE, "", "", 0.0, def.definitionLine});
        current = 0;
        currentSourceLine = def.definitionLine;
        parseExpression();

        tokens = std::move(savedTokens);
        current = savedCurrent;
        currentSourceLine = savedLine;
        return true;
    }

    void parsePrimary() {
        if (match(SyntaxTokenType::LEFT_PAREN)) {
            parseExpression();
            match(SyntaxTokenType::RIGHT_PAREN);
        } else if (match(SyntaxTokenType::NUMBER) || match(SyntaxTokenType::STRING)) {
            emit(OpCode::PUSH_VAL, tokens[current - 1].value);
        } else if (check(SyntaxTokenType::IDENTIFIER)) {
            std::string id = advance().lexeme;

            if (id.rfind("FN", 0) == 0 && emitUserFunctionCall(id)) {
                return;
            }

            if (id == "ERR") {
                if (match(SyntaxTokenType::LEFT_PAREN) && !match(SyntaxTokenType::RIGHT_PAREN)) {
                    reportCompileError("ERR takes no arguments.");
                }
                emit(OpCode::FN_ERR);
            }
            else if (id == "ERL") {
                if (match(SyntaxTokenType::LEFT_PAREN) && !match(SyntaxTokenType::RIGHT_PAREN)) {
                    reportCompileError("ERL takes no arguments.");
                }
                emit(OpCode::FN_ERL);
            }
            else if (id == "INKEY$") {
                if (match(SyntaxTokenType::LEFT_PAREN)) {
                    if (!match(SyntaxTokenType::RIGHT_PAREN)) {
                        reportCompileError("INKEY$ takes no arguments.");
                    }
                }
                emit(OpCode::FN_INKEY);
            }
            else if (id == "SQRT" || id == "ABS" || id == "RND" || id == "INT" || id == "LEN" || id == "POS" || id == "PEEK" || id == "USR" || id == "EOF") {
                if (match(SyntaxTokenType::LEFT_PAREN)) {
                    if (id == "POS") {
                        if (!check(SyntaxTokenType::RIGHT_PAREN)) {
                            parseExpression();
                            reportCompileError("POS() does not take any arguments.");
                        }
                    } else if (id == "EOF") {
                        if (check(SyntaxTokenType::RIGHT_PAREN)) {
                            reportCompileError("EOF() requires a channel argument.");
                            emit(OpCode::PUSH_VAL, 0.0);
                        } else {
                            parseExpression();
                        }
                    } else {
                        if (!check(SyntaxTokenType::RIGHT_PAREN)) parseExpression();
                        else emit(OpCode::PUSH_VAL, (id == "RND") ? 1.0 : 0.0);
                    }
                    match(SyntaxTokenType::RIGHT_PAREN);
                } else {
                    if (id == "POS") {
                        // POS is allowed as POS() and POS.
                    } else if (id == "EOF") {
                        reportCompileError("EOF requires parentheses and one channel argument.");
                        emit(OpCode::PUSH_VAL, 0.0);
                    } else if (id == "RND") {
                        emit(OpCode::PUSH_VAL, 1.0);
                    } else {
                        emit(OpCode::PUSH_VAL, 0.0);
                    }
                }
                if (id == "SQRT") emit(OpCode::FN_SQRT);
                else if (id == "ABS") emit(OpCode::FN_ABS);
                else if (id == "RND") emit(OpCode::FN_RND);
                else if (id == "INT") emit(OpCode::FN_INT);
                else if (id == "LEN") emit(OpCode::FN_LEN);
                else if (id == "POS") emit(OpCode::FN_POS);
                else if (id == "PEEK") emit(OpCode::FN_PEEK);
                else if (id == "USR") emit(OpCode::FN_USR);
                else if (id == "EOF") emit(OpCode::FN_EOF);
            } 
            else if (id == "LEFT$") {
                match(SyntaxTokenType::LEFT_PAREN);
                parseExpression(); match(SyntaxTokenType::COMMA);
                parseExpression(); match(SyntaxTokenType::RIGHT_PAREN);
                emit(OpCode::FN_LEFT);
            }
            else if (id == "RIGHT$") {
                match(SyntaxTokenType::LEFT_PAREN);
                parseExpression(); match(SyntaxTokenType::COMMA);
                parseExpression(); match(SyntaxTokenType::RIGHT_PAREN);
                emit(OpCode::FN_RIGHT);
            }
            else if (id == "MID$") {
                match(SyntaxTokenType::LEFT_PAREN);
                parseExpression(); match(SyntaxTokenType::COMMA);
                parseExpression();
                if (match(SyntaxTokenType::COMMA)) {
                    parseExpression();
                } else {
                    emit(OpCode::PUSH_VAL, -1.0);
                }
                match(SyntaxTokenType::RIGHT_PAREN);
                emit(OpCode::FN_MID);
            }
            else if (id == "CHR$") {
                match(SyntaxTokenType::LEFT_PAREN); parseExpression(); match(SyntaxTokenType::RIGHT_PAREN);
                emit(OpCode::FN_CHR);
            }
            else if (id == "ASC") {
                match(SyntaxTokenType::LEFT_PAREN); parseExpression(); match(SyntaxTokenType::RIGHT_PAREN);
                emit(OpCode::FN_ASC);
            }
            else if (id == "VAL") {
                match(SyntaxTokenType::LEFT_PAREN); parseExpression(); match(SyntaxTokenType::RIGHT_PAREN);
                emit(OpCode::FN_VAL);
            }
            else if (id == "STR$") {
                match(SyntaxTokenType::LEFT_PAREN); parseExpression(); match(SyntaxTokenType::RIGHT_PAREN);
                emit(OpCode::FN_STR);
            }
            else {
                int dims = 0;
                if (parseArraySubscripts(dims)) {
                    emit(OpCode::LOAD_ARRAY, getSymbolIndex(id), dims);
                } else {
                    emit(OpCode::LOAD_VAR, getSymbolIndex(id));
                }
            } 
        }
    }

    void parseUnary() {
        if (match(SyntaxTokenType::MINUS)) { parseUnary(); emit(OpCode::NEGATE); }
        else if (match(SyntaxTokenType::NOT)) { parseUnary(); emit(OpCode::LOGICAL_NOT); }
        else { parsePrimary(); }
    }

    void parsePower() {
        if (!enterPowerDepth()) {
            return;
        }
        parseUnary();
        if (match(SyntaxTokenType::CARET)) {
            // Right-associative exponentiation: A ^ B ^ C = A ^ (B ^ C)
            parsePower();
            emitBinaryOp(OpCode::POW);
        }
        leavePowerDepth();
    }

    void parseFactor() {
        parsePower();
        while (check(SyntaxTokenType::STAR) || check(SyntaxTokenType::SLASH) || check(SyntaxTokenType::BACKSLASH) || check(SyntaxTokenType::MOD)) {
            Token op = advance();
            parsePower();
            if (op.type == SyntaxTokenType::STAR) emitBinaryOp(OpCode::MUL);
            else if (op.type == SyntaxTokenType::SLASH) emitBinaryOp(OpCode::DIV);
            else if (op.type == SyntaxTokenType::BACKSLASH) emitBinaryOp(OpCode::INT_DIV);
            else if (op.type == SyntaxTokenType::MOD) emitBinaryOp(OpCode::MODULO);
        }
    }

    void parseTerm() {
        parseFactor();
        while (check(SyntaxTokenType::PLUS) || check(SyntaxTokenType::MINUS)) {
            Token op = advance();
            parseFactor();
            emitBinaryOp(op.type == SyntaxTokenType::PLUS ? OpCode::ADD : OpCode::SUB);
        }
    }

    void parseComparison() {
        parseTerm();
        if (check(SyntaxTokenType::EQUAL) || check(SyntaxTokenType::LESS) || check(SyntaxTokenType::GREATER) ||
            check(SyntaxTokenType::LESS_EQUAL) || check(SyntaxTokenType::GREATER_EQUAL) || check(SyntaxTokenType::NOT_EQUAL)) {
            Token op = advance();
            parseTerm();
            if (op.type == SyntaxTokenType::EQUAL) emit(OpCode::CMP_EQ);
            else if (op.type == SyntaxTokenType::LESS) emit(OpCode::CMP_LT);
            else if (op.type == SyntaxTokenType::GREATER) emit(OpCode::CMP_GT);
            else if (op.type == SyntaxTokenType::LESS_EQUAL) emit(OpCode::CMP_LE);
            else if (op.type == SyntaxTokenType::GREATER_EQUAL) emit(OpCode::CMP_GE);
            else if (op.type == SyntaxTokenType::NOT_EQUAL) emit(OpCode::CMP_NE);
        }
    }

    void parseLogicalAnd() {
        parseComparison();
        while (match(SyntaxTokenType::AND)) { parseComparison(); emit(OpCode::LOGICAL_AND); }
    }

    void parseLogicalOr() {
        parseLogicalAnd();
        while (match(SyntaxTokenType::OR)) { parseLogicalAnd(); emit(OpCode::LOGICAL_OR); }
    }

    void parseExpression() {
        if (!enterExpressionDepth()) {
            return;
        }
        parseLogicalOr();
        leaveExpressionDepth();
    }

    void parseStatement() {
        if (check(SyntaxTokenType::NUMBER)) {
            int lineNum = static_cast<int>(std::get<double>(advance().value));
            lineNumberToIpMap[lineNum] = code.size();
        }

        if (match(SyntaxTokenType::HELP)) emit(OpCode::SHOW_HELP);
        else if (match(SyntaxTokenType::VERSION)) emit(OpCode::SHOW_VERSION);
        else if (match(SyntaxTokenType::DEF)) {
            std::string fnName;
            if (check(SyntaxTokenType::IDENTIFIER) && peek().lexeme == "FN") {
                advance();
                if (check(SyntaxTokenType::IDENTIFIER)) {
                    fnName = "FN" + advance().lexeme;
                }
            } else if (check(SyntaxTokenType::IDENTIFIER)) {
                fnName = advance().lexeme;
            }

            if (fnName.empty() || fnName.rfind("FN", 0) != 0) {
                reportCompileError("DEF must define a function in the form DEF FNname(arg)=expr or DEF FN name(arg)=expr.");
                return;
            }

            if (!match(SyntaxTokenType::LEFT_PAREN) || !check(SyntaxTokenType::IDENTIFIER)) {
                reportCompileError("DEF " + fnName + " requires one parameter name in parentheses.");
                return;
            }

            std::string paramName = advance().lexeme;
            if (!match(SyntaxTokenType::RIGHT_PAREN) || !match(SyntaxTokenType::EQUAL)) {
                reportCompileError("DEF " + fnName + " must be followed by '(param)=expression'.");
                return;
            }

            UserFunctionDefinition def;
            def.name = fnName;
            def.paramName = paramName;
            def.paramSymbolIndex = getSymbolIndex(paramName);
            def.definitionLine = currentSourceLine;

            while (!check(SyntaxTokenType::NEWLINE) && !check(SyntaxTokenType::COLON) && !check(SyntaxTokenType::END_OF_FILE)) {
                def.bodyTokens.push_back(advance());
            }

            if (def.bodyTokens.empty()) {
                reportCompileError("DEF " + fnName + " must include an expression body.");
            } else {
                userFunctions[fnName] = std::move(def);
            }
        }
        else if (match(SyntaxTokenType::OPTION)) {
            if (!match(SyntaxTokenType::BASE)) {
                reportCompileError("Only OPTION BASE is supported.");
                return;
            }
            if (!check(SyntaxTokenType::NUMBER)) {
                reportCompileError("OPTION BASE requires numeric value 0 or 1.");
                return;
            }
            int base = static_cast<int>(std::get<double>(advance().value));
            if (base != 0 && base != 1) {
                reportCompileError("OPTION BASE only supports 0 or 1.");
                return;
            }
            emit(OpCode::SET_OPTION_BASE, base);
        }
        else if (match(SyntaxTokenType::ERASE)) {
            bool foundArray = false;
            do {
                if (!check(SyntaxTokenType::IDENTIFIER)) {
                    reportCompileError("ERASE requires one or more array names.");
                    break;
                }
                foundArray = true;
                emit(OpCode::ERASE_ARRAY, getSymbolIndex(advance().lexeme));
            } while (match(SyntaxTokenType::COMMA));

            if (!foundArray) {
                reportCompileError("ERASE requires at least one array name.");
            }
        }
        else if (match(SyntaxTokenType::OPEN)) {
            parseExpression();

            int mode = 1; // default OUTPUT
            if (match(SyntaxTokenType::FOR) || matchIdentifierLexeme("FOR")) {
                if (match(SyntaxTokenType::INPUT)) {
                    mode = 0;
                } else if (check(SyntaxTokenType::IDENTIFIER)) {
                    std::string m = advance().lexeme;
                    if (m == "OUTPUT") mode = 1;
                    else if (m == "APPEND") mode = 2;
                    else if (m == "RANDOM") mode = 3;
                    else {
                        reportCompileError("OPEN mode must be INPUT, OUTPUT, APPEND, or RANDOM.");
                        return;
                    }
                } else {
                    reportCompileError("OPEN mode must follow FOR.");
                    return;
                }
            }

            if (check(SyntaxTokenType::IDENTIFIER) && peek().lexeme == "AS") {
                advance();
            }
            parseExpression();

            bool lenProvided = false;
            if (mode == 3) {
                if (matchIdentifierLexeme("LEN")) {
                    if (!match(SyntaxTokenType::EQUAL)) {
                        reportCompileError("OPEN FOR RANDOM LEN requires '='.");
                        return;
                    }
                    parseExpression();
                    lenProvided = true;
                } else if (match(SyntaxTokenType::COMMA) && matchIdentifierLexeme("LEN")) {
                    if (!match(SyntaxTokenType::EQUAL)) {
                        reportCompileError("OPEN FOR RANDOM LEN requires '='.");
                        return;
                    }
                    parseExpression();
                    lenProvided = true;
                }
                if (!lenProvided) {
                    emit(OpCode::PUSH_VAL, 128.0);
                }
            }

            emit(OpCode::FILE_OPEN, 0.0, mode);
        }
        else if (match(SyntaxTokenType::CLOSE)) {
            if (check(SyntaxTokenType::NEWLINE) || check(SyntaxTokenType::COLON) || check(SyntaxTokenType::ELSE) || check(SyntaxTokenType::END_OF_FILE)) {
                emit(OpCode::FILE_CLOSE_ALL);
            } else {
                do {
                    parseExpression();
                    emit(OpCode::FILE_CLOSE);
                } while (match(SyntaxTokenType::COMMA));
            }
        }
        else if (match(SyntaxTokenType::PRINT_FILE)) {
            parseExpression();
            if (!match(SyntaxTokenType::COMMA)) {
                reportCompileError("PRINT# requires a channel expression followed by ','.");
                return;
            }

            auto isExpressionStart = [&]() {
                return check(SyntaxTokenType::NUMBER) || check(SyntaxTokenType::STRING) || check(SyntaxTokenType::IDENTIFIER) ||
                       check(SyntaxTokenType::LEFT_PAREN) || check(SyntaxTokenType::MINUS) || check(SyntaxTokenType::NOT);
            };

            int itemCount = 0;
            int pendingSep = 0; // 0=none, 1=';', 2=','
            std::vector<int> sepBeforeItem;
            while (!check(SyntaxTokenType::NEWLINE) && !check(SyntaxTokenType::COLON) && !check(SyntaxTokenType::ELSE) && !check(SyntaxTokenType::END_OF_FILE)) {
                if (!isExpressionStart()) {
                    reportCompileError("PRINT# has a separator with no following expression.");
                    break;
                }

                parseExpression();
                itemCount++;
                sepBeforeItem.push_back(pendingSep);
                pendingSep = 0;

                if (match(SyntaxTokenType::COMMA)) pendingSep = 2;
                else if (match(SyntaxTokenType::SEMICOLON)) pendingSep = 1;
                else break;
            }

            emit(OpCode::FILE_PRINT, static_cast<double>(pendingSep), itemCount);
            code.back().targets = std::move(sepBeforeItem);
        }
        else if (match(SyntaxTokenType::INPUT_FILE)) {
            parseExpression();
            if (!match(SyntaxTokenType::COMMA)) {
                reportCompileError("INPUT# requires a channel expression followed by variables.");
                return;
            }

            std::vector<int> targets;
            do {
                if (!check(SyntaxTokenType::IDENTIFIER)) {
                    reportCompileError("INPUT# requires variable names after channel.");
                    return;
                }
                targets.push_back(getSymbolIndex(advance().lexeme));
            } while (match(SyntaxTokenType::COMMA));

            emit(OpCode::FILE_INPUT, 0.0, static_cast<int>(targets.size()));
            code.back().targets = std::move(targets);
        }
        else if (match(SyntaxTokenType::CLS)) {
            emit(OpCode::CMD_CLS);
        }
        else if (match(SyntaxTokenType::LOCATE)) {
            parseExpression();
            if (!match(SyntaxTokenType::COMMA)) reportCompileError("LOCATE requires row and column separated by comma.");
            parseExpression();
            emit(OpCode::CMD_LOCATE);
        }
        else if (match(SyntaxTokenType::COLOR)) {
            parseExpression();
            if (match(SyntaxTokenType::COMMA)) {
                parseExpression();
            } else {
                emit(OpCode::PUSH_VAL, -1.0);
            }
            emit(OpCode::CMD_COLOR);
        }
        else if (match(SyntaxTokenType::SCREEN)) {
            parseExpression();
            emit(OpCode::CMD_SCREEN);
        }
        else if (match(SyntaxTokenType::PSET)) {
            parseExpression();
            if (!match(SyntaxTokenType::COMMA)) reportCompileError("PSET requires x,y coordinates.");
            parseExpression();
            int argc = 2;
            if (match(SyntaxTokenType::COMMA)) {
                parseExpression();
                argc = 3;
            }
            emit(OpCode::CMD_PSET, 0.0, argc);
        }
        else if (match(SyntaxTokenType::LINE)) {
            parseExpression();
            if (!match(SyntaxTokenType::COMMA)) reportCompileError("LINE requires x1,y1,x2,y2.");
            parseExpression();
            if (!match(SyntaxTokenType::COMMA)) reportCompileError("LINE requires x1,y1,x2,y2.");
            parseExpression();
            if (!match(SyntaxTokenType::COMMA)) reportCompileError("LINE requires x1,y1,x2,y2.");
            parseExpression();
            int argc = 4;
            if (match(SyntaxTokenType::COMMA)) {
                parseExpression();
                argc = 5;
            }
            emit(OpCode::CMD_LINE, 0.0, argc);
        }
        else if (match(SyntaxTokenType::PLAY)) {
            parseExpression();
            emit(OpCode::CMD_PLAY);
        }
        else if (match(SyntaxTokenType::SOUND)) {
            parseExpression();
            if (!match(SyntaxTokenType::COMMA)) reportCompileError("SOUND requires frequency and duration.");
            parseExpression();
            emit(OpCode::CMD_SOUND);
        }
        else if (match(SyntaxTokenType::FIELD)) {
            parseExpression();
            if (!match(SyntaxTokenType::COMMA)) {
                reportCompileError("FIELD requires channel, length AS var ...");
                return;
            }

            std::vector<int> targets;
            int fieldCount = 0;
            while (!check(SyntaxTokenType::NEWLINE) && !check(SyntaxTokenType::COLON) && !check(SyntaxTokenType::ELSE) && !check(SyntaxTokenType::END_OF_FILE)) {
                parseExpression();
                if (!(matchIdentifierLexeme("AS") || (check(SyntaxTokenType::IDENTIFIER) && peek().lexeme == "AS" && (advance(), true)))) {
                    reportCompileError("FIELD entry must be '<length> AS <string-variable>'.");
                    return;
                }
                if (!check(SyntaxTokenType::IDENTIFIER)) {
                    reportCompileError("FIELD requires variable name after AS.");
                    return;
                }
                std::string varName = advance().lexeme;
                targets.push_back(getSymbolIndex(varName));
                fieldCount++;
                if (!match(SyntaxTokenType::COMMA)) break;
            }

            emit(OpCode::FILE_RANDOM_FIELD, 0.0, fieldCount);
            code.back().targets = std::move(targets);
        }
        else if (match(SyntaxTokenType::GET)) {
            parseExpression();
            bool hasRecord = false;
            if (match(SyntaxTokenType::COMMA)) {
                parseExpression();
                hasRecord = true;
            }
            emit(OpCode::FILE_RANDOM_GET, hasRecord ? 1.0 : 0.0);
        }
        else if (match(SyntaxTokenType::PUT)) {
            parseExpression();
            bool hasRecord = false;
            if (match(SyntaxTokenType::COMMA)) {
                parseExpression();
                hasRecord = true;
            }
            emit(OpCode::FILE_RANDOM_PUT, hasRecord ? 1.0 : 0.0);
        }
        else if (match(SyntaxTokenType::PRINT)) {
            bool trailingSeparator = false;

            auto parsePrintItem = [&]() {
                if (check(SyntaxTokenType::IDENTIFIER) && peek().lexeme == "TAB" && peek(1).type == SyntaxTokenType::LEFT_PAREN) {
                    advance(); // TAB
                    match(SyntaxTokenType::LEFT_PAREN);
                    parseExpression();
                    match(SyntaxTokenType::RIGHT_PAREN);
                    emit(OpCode::PRINT_TAB_STOP);
                    return true;
                }

                parseExpression();
                emit(OpCode::PRINT_VAL);
                return true;
            };

            while (!check(SyntaxTokenType::NEWLINE) && !check(SyntaxTokenType::COLON) && !check(SyntaxTokenType::ELSE) && !check(SyntaxTokenType::END_OF_FILE)) {
                if (match(SyntaxTokenType::COMMA)) {
                    emit(OpCode::PRINT_TAB);
                    trailingSeparator = true;
                } else if (match(SyntaxTokenType::SEMICOLON)) {
                    trailingSeparator = true;
                } else {
                    parsePrintItem();
                    trailingSeparator = false;
                }
            }
            if (!trailingSeparator) emit(OpCode::PRINT_NEWLINE);
        } 
        else if (match(SyntaxTokenType::DIM)) {
            if (check(SyntaxTokenType::IDENTIFIER)) {
                std::string arrayName = advance().lexeme;
                int dims = 0;
                if (parseArraySubscripts(dims)) {
                    emit(OpCode::DIM_ARRAY, getSymbolIndex(arrayName), dims);
                }
            }
        }
        else if (match(SyntaxTokenType::DATA)) {
            while (!check(SyntaxTokenType::NEWLINE) && !check(SyntaxTokenType::COLON) && !check(SyntaxTokenType::ELSE) && !check(SyntaxTokenType::END_OF_FILE)) {
                if (match(SyntaxTokenType::NUMBER) || match(SyntaxTokenType::STRING)) {
                    dataValues.push_back(tokens[current - 1].value);
                } else if (check(SyntaxTokenType::IDENTIFIER)) {
                    dataValues.push_back(advance().rawLexeme);
                }
                match(SyntaxTokenType::COMMA);
            }
        }
        else if (match(SyntaxTokenType::READ)) {
            do {
                if (check(SyntaxTokenType::IDENTIFIER)) {
                    emit(OpCode::READ_VAR, getSymbolIndex(advance().lexeme));
                }
            } while (match(SyntaxTokenType::COMMA));
        }
        else if (match(SyntaxTokenType::RESTORE)) {
            emit(OpCode::RESTORE_DATA);
        }
        else if (match(SyntaxTokenType::POKE)) {
            parseExpression();
            if (!match(SyntaxTokenType::COMMA)) {
                reportCompileError("POKE requires address and value separated by a comma.");
            }
            parseExpression();
            emit(OpCode::POKE_MEM);
        }
        else if (match(SyntaxTokenType::LET) || check(SyntaxTokenType::IDENTIFIER)) {
            bool hadLet = false;
            if (check(SyntaxTokenType::LET)) {
                advance();
                hadLet = true;
            }
            if (!check(SyntaxTokenType::IDENTIFIER)) {
                reportCompileError(hadLet ? "LET requires a variable name." : "Expected statement after identifier.");
                return;
            }
            
            std::string varName = advance().lexeme;

            int dims = 0;
            if (parseArraySubscripts(dims)) {
                if (match(SyntaxTokenType::EQUAL)) {
                    parseExpression();
                    emit(OpCode::STORE_ARRAY, getSymbolIndex(varName), dims);
                } else {
                    reportCompileError("Expected '=' after array element reference for variable '" + varName + "'.");
                }
            } else if (match(SyntaxTokenType::EQUAL)) {
                parseExpression();
                emit(OpCode::STORE_VAR, getSymbolIndex(varName));
            } else {
                reportCompileError("Expected '=' or '(' after variable name '" + varName + "'.");
            }
        } 
        else if (match(SyntaxTokenType::INPUT)) {
            bool promptProvided = false;
            bool promptCommaStyle = false;

            if (check(SyntaxTokenType::STRING)) {
                emit(OpCode::PUSH_VAL, advance().value);
                promptProvided = true;
                if (match(SyntaxTokenType::SEMICOLON)) {
                    promptCommaStyle = false;
                } else if (match(SyntaxTokenType::COMMA)) {
                    promptCommaStyle = true;
                } else {
                    reportCompileError("INPUT prompt must be followed by ';' or ','.");
                    return;
                }
            }

            std::vector<int> targets;
            do {
                if (!check(SyntaxTokenType::IDENTIFIER)) {
                    reportCompileError("INPUT requires one or more variable names.");
                    return;
                }
                targets.push_back(getSymbolIndex(advance().lexeme));
            } while (match(SyntaxTokenType::COMMA));

            if (targets.empty()) {
                reportCompileError("INPUT requires at least one variable.");
                return;
            }

            int mode = promptProvided ? (promptCommaStyle ? 2 : 1) : 0;
            emit(OpCode::INPUT, static_cast<double>(mode), static_cast<int>(targets.size()));
            code.back().targets = std::move(targets);
        } 
        else if (match(SyntaxTokenType::GOTO)) {
            if (check(SyntaxTokenType::NUMBER)) {
                int targetLine = static_cast<int>(std::get<double>(advance().value));
                pendingJumps.push_back({code.size(), targetLine});
                emit(OpCode::JUMP, 0.0, -1);
            }
        } 
        else if (match(SyntaxTokenType::GOSUB)) {
            if (check(SyntaxTokenType::NUMBER)) {
                int targetLine = static_cast<int>(std::get<double>(advance().value));
                pendingJumps.push_back({code.size(), targetLine});
                emit(OpCode::CALL, 0.0, -1);
            }
        }
        else if (match(SyntaxTokenType::ON)) {
            if (check(SyntaxTokenType::IDENTIFIER) && peek().lexeme == "ERROR") {
                advance(); // ERROR
                if (!match(SyntaxTokenType::GOTO)) {
                    reportCompileError("ON ERROR requires GOTO <line-number>.");
                    return;
                }
                if (!check(SyntaxTokenType::NUMBER)) {
                    reportCompileError("ON ERROR GOTO requires a line number.");
                    return;
                }
                int targetLine = static_cast<int>(std::get<double>(advance().value));
                if (targetLine == 0) {
                    emit(OpCode::SET_ON_ERROR, 0.0, -1);
                } else {
                    pendingJumps.push_back({code.size(), targetLine});
                    emit(OpCode::SET_ON_ERROR, 1.0, -1);
                }
                return;
            }

            parseExpression();
            bool isGosub = false;
            if (match(SyntaxTokenType::GOTO)) isGosub = false;
            else if (match(SyntaxTokenType::GOSUB)) isGosub = true;

            std::vector<int> targetLines;
            do {
                if (check(SyntaxTokenType::NUMBER)) {
                    targetLines.push_back(static_cast<int>(std::get<double>(advance().value)));
                }
            } while (match(SyntaxTokenType::COMMA));

            size_t instIdx = code.size();
            emit(isGosub ? OpCode::ON_GOSUB : OpCode::ON_GOTO, static_cast<double>(targetLines.size()));
            code.back().targets.resize(targetLines.size(), -1);

            for (size_t i = 0; i < targetLines.size(); ++i) {
                pendingOnJumps.push_back({instIdx, i, targetLines[i]});
            }
        }
        else if (match(SyntaxTokenType::RESUME)) {
            if (match(SyntaxTokenType::NEXT)) {
                emit(OpCode::RESUME_ERROR, 1.0, -1);
            } else if (check(SyntaxTokenType::NUMBER)) {
                int targetLine = static_cast<int>(std::get<double>(advance().value));
                pendingJumps.push_back({code.size(), targetLine});
                emit(OpCode::RESUME_ERROR, 2.0, -1);
            } else {
                emit(OpCode::RESUME_ERROR, 0.0, -1);
            }
        }
        else if (match(SyntaxTokenType::RETURN)) emit(OpCode::RETURN);
        else if (match(SyntaxTokenType::FOR)) {
            if (check(SyntaxTokenType::IDENTIFIER)) {
                std::string varName = advance().lexeme;
                if (match(SyntaxTokenType::EQUAL)) {
                    parseExpression();
                    emit(OpCode::STORE_VAR, getSymbolIndex(varName));

                    if (match(SyntaxTokenType::TO)) {
                        parseExpression();
                        if (match(SyntaxTokenType::STEP)) {
                            parseExpression();
                        } else {
                            emit(OpCode::PUSH_VAL, 1.0);
                        }

                        size_t forInitIp = code.size();
                        emit(OpCode::FOR_INIT, getSymbolIndex(varName), -1);
                        size_t loopStartIp = code.size();

                        forStack.push_back({varName, loopStartIp, forInitIp});
                    }
                }
            }
        }
        else if (match(SyntaxTokenType::NEXT)) {
            std::string varName = "";
            if (check(SyntaxTokenType::IDENTIFIER)) varName = advance().lexeme;

            if (forStack.empty()) {
                reportCompileError("NEXT without matching FOR.");
            } else if (varName.empty()) {
                auto loop = forStack.back();
                forStack.pop_back();

                emit(OpCode::FOR_NEXT, getSymbolIndex(loop.varName), static_cast<int>(loop.loopStartIp));
                code[loop.forInitIp].targetIp = static_cast<int>(code.size());
            } else {
                const auto& loop = forStack.back();
                if (loop.varName != varName) {
                    reportCompileError("NEXT " + varName + " mismatches innermost FOR " + loop.varName + ".");
                } else {
                    CompileTimeForLoop matchedLoop = loop;
                    forStack.pop_back();

                    emit(OpCode::FOR_NEXT, getSymbolIndex(varName), static_cast<int>(matchedLoop.loopStartIp));
                    code[matchedLoop.forInitIp].targetIp = static_cast<int>(code.size());
                }
            }
        }
        else if (match(SyntaxTokenType::IF)) {
            auto parseInlineBranch = [&](bool stopAtElse) {
                parseStatement();
                while (match(SyntaxTokenType::COLON)) {
                    if (check(SyntaxTokenType::NEWLINE) || check(SyntaxTokenType::END_OF_FILE) || (stopAtElse && check(SyntaxTokenType::ELSE))) break;
                    parseStatement();
                }
            };

            parseExpression();
            if (match(SyntaxTokenType::THEN)) {
                size_t jumpIfFalseIp = code.size();
                emit(OpCode::JUMP_IF_FALSE, 0.0, -1);

                bool thenIsLineNumber = false;
                if (check(SyntaxTokenType::NUMBER)) {
                    thenIsLineNumber = true;
                    int thenTargetLine = static_cast<int>(std::get<double>(advance().value));
                    pendingJumps.push_back({code.size(), thenTargetLine});
                    emit(OpCode::JUMP, 0.0, -1);
                } else {
                    parseInlineBranch(true);
                }

                if (match(SyntaxTokenType::ELSE)) {
                    if (thenIsLineNumber) {
                        code[jumpIfFalseIp].targetIp = static_cast<int>(code.size());

                        if (check(SyntaxTokenType::NUMBER)) {
                            int elseTargetLine = static_cast<int>(std::get<double>(advance().value));
                            pendingJumps.push_back({code.size(), elseTargetLine});
                            emit(OpCode::JUMP, 0.0, -1);
                        } else {
                            parseInlineBranch(false);
                        }
                    } else {
                        size_t jumpOverElseIp = code.size();
                        emit(OpCode::JUMP, 0.0, -1);

                        code[jumpIfFalseIp].targetIp = static_cast<int>(code.size());
                        if (check(SyntaxTokenType::NUMBER)) {
                            int elseTargetLine = static_cast<int>(std::get<double>(advance().value));
                            pendingJumps.push_back({code.size(), elseTargetLine});
                            emit(OpCode::JUMP, 0.0, -1);
                        } else {
                            parseInlineBranch(false);
                        }

                        code[jumpOverElseIp].targetIp = static_cast<int>(code.size());
                    }
                } else {
                    code[jumpIfFalseIp].targetIp = static_cast<int>(code.size());
                }
            }
        }
        else if (match(SyntaxTokenType::ELSE)) {
            // Ignore stray ELSE tokens so the parser always advances.
        }
        else if (match(SyntaxTokenType::END)) emit(OpCode::HALT);
    }

public:
    Compiler(std::vector<Token> tok) : tokens(std::move(tok)) {}

    std::tuple<std::vector<Instruction>, std::vector<Value>, std::vector<std::string>> compile() {
        while (!isAtEnd()) {
            while (match(SyntaxTokenType::NEWLINE) || match(SyntaxTokenType::COLON));
            if (isAtEnd()) break;
            parseStatement();
        }
        emit(OpCode::HALT);

        bool compileError = false;
        for (const auto& err : compileErrors) {
            printErrorLine(err);
            compileError = true;
        }

        if (!forStack.empty()) {
            for (const auto& loop : forStack) {
                printErrorLine("Compilation Error: FOR " + loop.varName + " is missing NEXT.");
            }
            compileError = true;
        }

        for (const auto& jump : pendingJumps) {
            size_t instructionIndex = jump.first;
            int targetLine = jump.second;
            if (lineNumberToIpMap.count(targetLine)) {
                code[instructionIndex].targetIp = static_cast<int>(lineNumberToIpMap[targetLine]);
            } else {
                printErrorLine("Compilation Error: Line number " + std::to_string(targetLine) + " not found.");
                compileError = true;
            }
        }

        for (const auto& onJump : pendingOnJumps) {
            if (lineNumberToIpMap.count(onJump.targetLine)) {
                code[onJump.instructionIndex].targets[onJump.tableIndex] = static_cast<int>(lineNumberToIpMap[onJump.targetLine]);
            } else {
                printErrorLine("Compilation Error: Line number " + std::to_string(onJump.targetLine) + " in ON statement not found.");
                compileError = true;
            }
        }

        if (compileError) {
            code.clear();
            emit(OpCode::HALT);
        }

        return {code, dataValues, symbolTable};
    }
};

void dumpBytecode(const std::vector<Instruction>& code, const std::vector<Value>& dataValues, const std::vector<std::string>& symbolTable) {
    std::cout << "--- BYTECODE DISASSEMBLY (" << code.size() << " instructions) ---\n";
    for (size_t i = 0; i < code.size(); ++i) {
        const auto& inst = code[i];
        std::cout << (i < 10 ? "00" : (i < 100 ? "0" : "")) << i << " [Line " << inst.line << "]: ";
        
        auto getSym = [&](const Value& val) -> std::string {
            int idx = static_cast<int>(asDouble(val));
            return (idx >= 0 && idx < static_cast<int>(symbolTable.size())) ? symbolTable[idx] : std::to_string(idx);
        };

        switch (inst.op) {
            case OpCode::PUSH_VAL: std::cout << "PUSH_VAL " << valueToString(inst.operand); break;
            case OpCode::LOAD_VAR: std::cout << "LOAD_VAR " << getSym(inst.operand) << " [Offset " << asDouble(inst.operand) << "]"; break;
            case OpCode::STORE_VAR: std::cout << "STORE_VAR " << getSym(inst.operand) << " [Offset " << asDouble(inst.operand) << "]"; break;
            case OpCode::DIM_ARRAY: std::cout << "DIM_ARRAY " << getSym(inst.operand) << " (" << inst.targetIp << "D)"; break;
            case OpCode::LOAD_ARRAY: std::cout << "LOAD_ARRAY " << getSym(inst.operand) << " (" << inst.targetIp << "D)"; break;
            case OpCode::STORE_ARRAY: std::cout << "STORE_ARRAY " << getSym(inst.operand) << " (" << inst.targetIp << "D)"; break;
            case OpCode::SET_OPTION_BASE: std::cout << "SET_OPTION_BASE " << asDouble(inst.operand); break;
            case OpCode::ERASE_ARRAY: std::cout << "ERASE_ARRAY " << getSym(inst.operand); break;
            case OpCode::FILE_OPEN: std::cout << "FILE_OPEN mode=" << inst.targetIp; break;
            case OpCode::FILE_CLOSE: std::cout << "FILE_CLOSE"; break;
            case OpCode::FILE_CLOSE_ALL: std::cout << "FILE_CLOSE_ALL"; break;
            case OpCode::FILE_PRINT: std::cout << "FILE_PRINT items=" << inst.targetIp << " trailingSep=" << asDouble(inst.operand); break;
            case OpCode::FILE_INPUT: std::cout << "FILE_INPUT vars=" << inst.targetIp; break;
            case OpCode::FILE_RANDOM_FIELD: std::cout << "FILE_RANDOM_FIELD fields=" << inst.targetIp; break;
            case OpCode::FILE_RANDOM_GET: std::cout << "FILE_RANDOM_GET hasRecord=" << asDouble(inst.operand); break;
            case OpCode::FILE_RANDOM_PUT: std::cout << "FILE_RANDOM_PUT hasRecord=" << asDouble(inst.operand); break;
            case OpCode::SET_ON_ERROR: std::cout << "SET_ON_ERROR enabled=" << asDouble(inst.operand) << " target=" << inst.targetIp; break;
            case OpCode::RESUME_ERROR: std::cout << "RESUME_ERROR mode=" << asDouble(inst.operand) << " target=" << inst.targetIp; break;
            case OpCode::CMD_CLS: std::cout << "CMD_CLS"; break;
            case OpCode::CMD_LOCATE: std::cout << "CMD_LOCATE"; break;
            case OpCode::CMD_COLOR: std::cout << "CMD_COLOR"; break;
            case OpCode::CMD_SCREEN: std::cout << "CMD_SCREEN"; break;
            case OpCode::CMD_LINE: std::cout << "CMD_LINE argc=" << inst.targetIp; break;
            case OpCode::CMD_PSET: std::cout << "CMD_PSET argc=" << inst.targetIp; break;
            case OpCode::CMD_PLAY: std::cout << "CMD_PLAY"; break;
            case OpCode::CMD_SOUND: std::cout << "CMD_SOUND"; break;
            case OpCode::ADD: std::cout << "ADD"; break;
            case OpCode::SUB: std::cout << "SUB"; break;
            case OpCode::MUL: std::cout << "MUL"; break;
            case OpCode::DIV: std::cout << "DIV"; break;
            case OpCode::INT_DIV: std::cout << "INT_DIV"; break;
            case OpCode::MODULO: std::cout << "MODULO"; break;
            case OpCode::POW: std::cout << "POW"; break;
            case OpCode::NEGATE: std::cout << "NEGATE"; break;
            case OpCode::CMP_EQ: std::cout << "CMP_EQ"; break;
            case OpCode::CMP_LT: std::cout << "CMP_LT"; break;
            case OpCode::CMP_GT: std::cout << "CMP_GT"; break;
            case OpCode::CMP_LE: std::cout << "CMP_LE"; break;
            case OpCode::CMP_GE: std::cout << "CMP_GE"; break;
            case OpCode::CMP_NE: std::cout << "CMP_NE"; break;
            case OpCode::LOGICAL_AND: std::cout << "LOGICAL_AND"; break;
            case OpCode::LOGICAL_OR: std::cout << "LOGICAL_OR"; break;
            case OpCode::LOGICAL_NOT: std::cout << "LOGICAL_NOT"; break;
            case OpCode::PRINT_VAL: std::cout << "PRINT_VAL"; break;
            case OpCode::PRINT_TAB: std::cout << "PRINT_TAB"; break;
            case OpCode::PRINT_TAB_STOP: std::cout << "PRINT_TAB_STOP"; break;
            case OpCode::PRINT_NEWLINE: std::cout << "PRINT_NEWLINE"; break;
            case OpCode::INPUT: std::cout << "INPUT vars=" << inst.targetIp << " mode=" << asDouble(inst.operand); break;
            case OpCode::JUMP: std::cout << "JUMP -> " << inst.targetIp; break;
            case OpCode::JUMP_IF_FALSE: std::cout << "JUMP_IF_FALSE -> " << inst.targetIp; break;
            case OpCode::ON_GOTO: {
                std::cout << "ON_GOTO Targets: [";
                for (size_t t = 0; t < inst.targets.size(); ++t) std::cout << inst.targets[t] << (t + 1 < inst.targets.size() ? ", " : "");
                std::cout << "]";
                break;
            }
            case OpCode::ON_GOSUB: {
                std::cout << "ON_GOSUB Targets: [";
                for (size_t t = 0; t < inst.targets.size(); ++t) std::cout << inst.targets[t] << (t + 1 < inst.targets.size() ? ", " : "");
                std::cout << "]";
                break;
            }
            case OpCode::FOR_INIT: std::cout << "FOR_INIT " << getSym(inst.operand) << " (Exit -> " << inst.targetIp << ")"; break;
            case OpCode::FOR_NEXT: std::cout << "FOR_NEXT " << getSym(inst.operand) << " (Loop -> " << inst.targetIp << ")"; break;
            case OpCode::READ_VAR: std::cout << "READ_VAR " << getSym(inst.operand); break;
            case OpCode::RESTORE_DATA: std::cout << "RESTORE_DATA"; break;
            case OpCode::POKE_MEM: std::cout << "POKE_MEM"; break;
            case OpCode::CALL: std::cout << "CALL -> " << inst.targetIp; break;
            case OpCode::RETURN: std::cout << "RETURN"; break;
            case OpCode::FN_SQRT: std::cout << "FN_SQRT"; break;
            case OpCode::FN_ABS: std::cout << "FN_ABS"; break;
            case OpCode::FN_RND: std::cout << "FN_RND"; break;
            case OpCode::FN_INT: std::cout << "FN_INT"; break;
            case OpCode::FN_LEN: std::cout << "FN_LEN"; break;
            case OpCode::FN_POS: std::cout << "FN_POS"; break;
            case OpCode::FN_PEEK: std::cout << "FN_PEEK"; break;
            case OpCode::FN_USR: std::cout << "FN_USR"; break;
            case OpCode::FN_INKEY: std::cout << "FN_INKEY"; break;
            case OpCode::FN_ERR: std::cout << "FN_ERR"; break;
            case OpCode::FN_ERL: std::cout << "FN_ERL"; break;
            case OpCode::FN_EOF: std::cout << "FN_EOF"; break;
            case OpCode::FN_LEFT: std::cout << "FN_LEFT"; break;
            case OpCode::FN_RIGHT: std::cout << "FN_RIGHT"; break;
            case OpCode::FN_MID: std::cout << "FN_MID"; break;
            case OpCode::FN_CHR: std::cout << "FN_CHR"; break;
            case OpCode::FN_ASC: std::cout << "FN_ASC"; break;
            case OpCode::FN_VAL: std::cout << "FN_VAL"; break;
            case OpCode::FN_STR: std::cout << "FN_STR"; break;
            case OpCode::SHOW_HELP: std::cout << "SHOW_HELP"; break;
            case OpCode::SHOW_VERSION: std::cout << "SHOW_VERSION"; break;
            case OpCode::HALT: std::cout << "HALT"; break;
        }
        std::cout << "\n";
    }
    if (!symbolTable.empty()) {
        std::cout << "--- SYMBOL TABLE (" << symbolTable.size() << " variables) ---\n";
        for (size_t i = 0; i < symbolTable.size(); ++i) {
            std::cout << "Offset [" << i << "]: " << symbolTable[i] << "\n";
        }
    }
    if (!dataValues.empty()) {
        std::cout << "--- DATA BUFFER (" << dataValues.size() << " items) ---\n";
        for (size_t i = 0; i < dataValues.size(); ++i) {
            std::cout << "[" << i << "] " << valueToString(dataValues[i]) << "  ";
        }
        std::cout << "\n";
    }
    std::cout << "-------------------------------------\n";
}

// ============================================================================
// 4. VIRTUAL MACHINE INTERPRETER
// ============================================================================
struct VMForLoop {
    int varIdx;
    double limit;
    double step;
    int startIp;
    int endIpExclusive;
};

struct ArrayData {
    std::vector<int> extents;
    std::vector<Value> data;
};

struct BasicFileHandle {
    std::ifstream in;
    std::ofstream out;
    std::fstream random;
    bool forInput = false;
    bool forOutput = false;
    bool forRandom = false;
    int recordLen = 128;
    long long currentRecord = 1;
    struct FieldBinding {
        int varIdx = -1;
        int length = 0;
        int offset = 0;
    };
    std::vector<FieldBinding> fields;
};

struct RuntimeErrorSignal {
    std::string message;
    int line = 0;
};

struct CsvField {
    std::string text;
    bool quoted = false;
};

class VirtualMachine {
    std::vector<Instruction> code;
    std::vector<Value> dataValues;
    std::vector<std::string> symbolNames;
    size_t dataPtr = 0;

    std::vector<Value> globalVariables;
    std::unordered_map<int, ArrayData> arrays;
    std::unordered_map<int, int> virtualMemory;
    std::unordered_map<int, BasicFileHandle> openFiles;
    int optionBase = 0;
    int cursorRow = 1;
    int cursorCol = 1;
    int colorFg = 7;
    int colorBg = 0;
    int screenMode = 0;
    bool onErrorEnabled = false;
    int onErrorTargetIp = -1;
    bool errorActive = false;
    int errorResumeIp = -1;
    int errorResumeNextIp = -1;
    int lastErrorCode = 0;
    int lastErrorLine = 0;
    
    std::vector<Value> stack;
    std::vector<size_t> callStack;
    std::vector<VMForLoop> forLoopStack;
    size_t ip = 0;
    int printColumn = 0;
    double lastRndValue = 0.0;
    size_t maxInstructionSteps = 1000000;
    std::function<bool(size_t, const Instruction&)> stepHook;

    void printSpaces(int count) {
        for (int i = 0; i < count; ++i) std::cout << ' ';
        printColumn += count;
    }

    void unwindForLoopsForJump(int targetIp) {
        while (!forLoopStack.empty()) {
            const auto& loop = forLoopStack.back();
            if (targetIp >= loop.startIp && targetIp < loop.endIpExclusive) {
                break;
            }
            forLoopStack.pop_back();
        }
    }

    void push(Value val) { stack.push_back(val); }
    Value pop() {
        if (stack.empty()) return 0.0;
        Value val = stack.back();
        stack.pop_back();
        return val;
    }

    void setVariable(int idx, Value val) {
        if (idx >= static_cast<int>(globalVariables.size())) {
            globalVariables.resize(idx + 1, 0.0);
        }
        globalVariables[idx] = val;
    }

    bool variableExpectsString(int idx) const {
        if (idx < 0 || idx >= static_cast<int>(symbolNames.size())) return false;
        const std::string& n = symbolNames[static_cast<size_t>(idx)];
        return !n.empty() && n.back() == '$';
    }

    void assignInputFieldToVariable(int varIdx, const CsvField& field, int line, const char* opName) {
        if (variableExpectsString(varIdx)) {
            setVariable(varIdx, field.text);
            return;
        }

        size_t parsed = 0;
        try {
            double dv = std::stod(field.text, &parsed);
            if (parsed == field.text.size()) {
                setVariable(varIdx, dv);
                return;
            }
        } catch (...) {
        }

        runtimeError(std::string("Type mismatch in ") + opName + " for numeric variable", line);
    }

    Value getVariable(int idx) {
        if (idx >= 0 && idx < static_cast<int>(globalVariables.size())) {
            return globalVariables[idx];
        }
        return 0.0;
    }

    void runtimeError(const std::string& msg, int line) {
        throw RuntimeErrorSignal{msg, line};
    }

    int classifyErrorCode(const std::string& message) const {
        if (message.find("Division by zero") != std::string::npos ||
            message.find("Integer division by zero") != std::string::npos ||
            message.find("Modulo by zero") != std::string::npos) {
            return 11;
        }
        if (message.find("Array index out of bounds") != std::string::npos ||
            message.find("Array upper bound") != std::string::npos ||
            message.find("Array dimension mismatch") != std::string::npos ||
            message.find("Array is too large") != std::string::npos ||
            message.find("Out of memory while allocating array") != std::string::npos) {
            return 9;
        }
        if (message.find("Out of DATA") != std::string::npos) {
            return 4;
        }
        if (message.find("OPEN failed") != std::string::npos ||
            message.find("sandbox violation") != std::string::npos ||
            message.find("channel") != std::string::npos ||
            message.find("RANDOM") != std::string::npos ||
            message.find("FIELD") != std::string::npos ||
            message.find("GET ") != std::string::npos ||
            message.find("PUT ") != std::string::npos) {
            return 52;
        }
        return 5;
    }

    bool isNumber(const Value& val) const {
        return std::holds_alternative<double>(val);
    }

    bool requireNumericUnary(const Value& val, const char* op, int line) {
        if (isNumber(val)) return true;
        runtimeError(std::string("Type mismatch in '") + op + "' operation", line);
        return false;
    }

    bool requireNumericBinary(const Value& a, const Value& b, const char* op, int line) {
        if (isNumber(a) && isNumber(b)) return true;
        runtimeError(std::string("Type mismatch in '") + op + "' operation", line);
        return false;
    }

    bool parseNumericValue(const Value& val, bool& ok) {
        ok = false;
        if (std::holds_alternative<double>(val)) {
            ok = true;
            return std::get<double>(val);
        }
        if (std::holds_alternative<std::string>(val)) {
            const std::string& text = std::get<std::string>(val);
            std::size_t parsed = 0;
            try {
                double converted = std::stod(text, &parsed);
                if (parsed == text.size()) {
                    ok = true;
                    return converted;
                }
            } catch (...) {
            }
        }
        return 0.0;
    }

    bool compareValues(const Value& a, const Value& b, const char* op, int line, bool& result) {
        bool bothNumeric = std::holds_alternative<double>(a) && std::holds_alternative<double>(b);
        bool bothString = std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b);
        if (bothNumeric) {
            double av = std::get<double>(a);
            double bv = std::get<double>(b);
            if (std::string(op) == "=") result = av == bv;
            else if (std::string(op) == "<") result = av < bv;
            else if (std::string(op) == ">") result = av > bv;
            else if (std::string(op) == "<=") result = av <= bv;
            else if (std::string(op) == ">=") result = av >= bv;
            else if (std::string(op) == "<>") result = av != bv;
            else result = false;
            return true;
        }

        if (bothString) {
            const std::string& av = std::get<std::string>(a);
            const std::string& bv = std::get<std::string>(b);
            if (std::string(op) == "=") result = av == bv;
            else if (std::string(op) == "<") result = av < bv;
            else if (std::string(op) == ">") result = av > bv;
            else if (std::string(op) == "<=") result = av <= bv;
            else if (std::string(op) == ">=") result = av >= bv;
            else if (std::string(op) == "<>") result = av != bv;
            else result = false;
            return true;
        }

        bool aNumericOk = false;
        bool bNumericOk = false;
        double av = parseNumericValue(a, aNumericOk);
        double bv = parseNumericValue(b, bNumericOk);
        if (aNumericOk && bNumericOk) {
            if (std::string(op) == "=") result = av == bv;
            else if (std::string(op) == "<") result = av < bv;
            else if (std::string(op) == ">") result = av > bv;
            else if (std::string(op) == "<=") result = av <= bv;
            else if (std::string(op) == ">=") result = av >= bv;
            else if (std::string(op) == "<>") result = av != bv;
            else result = false;
            return true;
        }

        if (std::string(op) == "<>") {
            result = true;
            return true;
        }

        result = false;
        return true;
    }

    std::vector<int> popArrayIndices(int dims) {
        std::vector<int> idx(dims, 0);
        for (int i = dims - 1; i >= 0; --i) {
            idx[i] = static_cast<int>(std::lround(asDouble(pop())));
        }
        return idx;
    }

    std::vector<int> popArrayUpperBounds(int dims) {
        std::vector<int> bounds(dims, 0);
        for (int i = dims - 1; i >= 0; --i) {
            bounds[i] = static_cast<int>(std::lround(asDouble(pop())));
        }
        return bounds;
    }

    int flattenArrayIndex(const ArrayData& arr, const std::vector<int>& indices, int line, bool& ok) {
        ok = true;
        if (indices.size() != arr.extents.size()) {
            runtimeError("Array dimension mismatch", line);
            ok = false;
            return 0;
        }

        int linear = 0;
        int stride = 1;
        for (int i = static_cast<int>(arr.extents.size()) - 1; i >= 0; --i) {
            int internalIdx = indices[i] - optionBase;
            if (internalIdx < 0 || internalIdx >= arr.extents[i]) {
                runtimeError("Array index out of bounds", line);
                ok = false;
                return 0;
            }
            linear += internalIdx * stride;
            stride *= arr.extents[i];
        }
        return linear;
    }

    ArrayData makeArrayFromUpperBounds(const std::vector<int>& upperBounds, int line, bool& ok) {
        ok = true;
        ArrayData arr;
        try {
            arr.extents.reserve(upperBounds.size());

            size_t total = 1;
            for (int bound : upperBounds) {
                if (bound < optionBase) {
                    runtimeError("Array upper bound is below OPTION BASE", line);
                    ok = false;
                    return arr;
                }
                int extent = (bound - optionBase) + 1;
                if (extent <= 0) {
                    runtimeError("Array upper bound is invalid", line);
                    ok = false;
                    return arr;
                }
                if (total > (std::numeric_limits<size_t>::max() / static_cast<size_t>(extent))) {
                    runtimeError("Array is too large", line);
                    ok = false;
                    return arr;
                }
                arr.extents.push_back(extent);
                total *= static_cast<size_t>(extent);
            }
            arr.data.assign(total, 0.0);
        } catch (const std::bad_alloc&) {
            runtimeError("Out of memory while allocating array", line);
            ok = false;
        }
        return arr;
    }

    ArrayData makeImplicitArray(int dims, int line) {
        int defaultUpper = 10;
        int extent = (defaultUpper >= optionBase) ? ((defaultUpper - optionBase) + 1) : 1;
        ArrayData arr;
        try {
            arr.extents.assign(dims, extent);

            size_t total = 1;
            for (int i = 0; i < dims; ++i) {
                if (total > (std::numeric_limits<size_t>::max() / static_cast<size_t>(extent))) {
                    runtimeError("Array is too large", line);
                    return arr;
                }
                total *= static_cast<size_t>(extent);
            }
            arr.data.assign(total, 0.0);
        } catch (const std::bad_alloc&) {
            runtimeError("Out of memory while allocating array", line);
        }
        return arr;
    }

    int asChannel(const Value& v) {
        return static_cast<int>(std::lround(asDouble(v)));
    }

    int asCheckedInt(const Value& v, int line, const char* what, int minValue, int maxValue) {
        double d = asDouble(v);
        if (!std::isfinite(d)) {
            runtimeError(std::string(what) + " must be a finite number", line);
        }
        double rounded = std::round(d);
        if (std::fabs(d - rounded) > 1e-9) {
            runtimeError(std::string(what) + " must be an integer", line);
        }
        long long ll = static_cast<long long>(rounded);
        if (ll < static_cast<long long>(minValue) || ll > static_cast<long long>(maxValue)) {
            runtimeError(std::string(what) + " is out of supported range", line);
        }
        return static_cast<int>(ll);
    }

    int asCheckedPositiveChannel(const Value& v, int line, const char* opName) {
        int channel = asCheckedInt(v, line, "Channel", 1, std::numeric_limits<int>::max());
        (void)opName;
        return channel;
    }

    static std::string trim(const std::string& s) {
        size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, (last - first + 1));
    }

    static std::vector<CsvField> splitCsvSimple(const std::string& line) {
        std::vector<CsvField> fields;
        std::string cur;
        bool inQuotes = false;
        bool fieldQuoted = false;
        bool atFieldStart = true;

        for (size_t i = 0; i < line.size(); ++i) {
            char ch = line[i];
            if (ch == '"') {
                if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                    cur.push_back('"');
                    i++;
                    atFieldStart = false;
                    continue;
                }

                if (!inQuotes && atFieldStart) {
                    fieldQuoted = true;
                    inQuotes = true;
                    continue;
                }

                inQuotes = !inQuotes;
                continue;
            }

            if (ch == ',' && !inQuotes) {
                CsvField f;
                f.quoted = fieldQuoted;
                f.text = fieldQuoted ? cur : trim(cur);
                fields.push_back(std::move(f));

                cur.clear();
                inQuotes = false;
                fieldQuoted = false;
                atFieldStart = true;
                continue;
            }

            cur.push_back(ch);
            if (!std::isspace(static_cast<unsigned char>(ch)) || inQuotes || !atFieldStart) {
                atFieldStart = false;
            }
        }

        CsvField f;
        f.quoted = fieldQuoted;
        f.text = fieldQuoted ? cur : trim(cur);
        fields.push_back(std::move(f));
        return fields;
    }

    static std::string toFixedWidth(std::string value, int width) {
        if (width <= 0) return "";
        if (static_cast<int>(value.size()) > width) {
            value = value.substr(0, static_cast<size_t>(width));
        } else if (static_cast<int>(value.size()) < width) {
            value.append(static_cast<size_t>(width - static_cast<int>(value.size())), ' ');
        }
        return value;
    }

    void closeChannel(int channel) {
        auto it = openFiles.find(channel);
        if (it == openFiles.end()) return;
        if (it->second.in.is_open()) it->second.in.close();
        if (it->second.out.is_open()) it->second.out.close();
        if (it->second.random.is_open()) it->second.random.close();
        openFiles.erase(it);
    }

    void closeAllChannels() {
        for (auto& kv : openFiles) {
            if (kv.second.in.is_open()) kv.second.in.close();
            if (kv.second.out.is_open()) kv.second.out.close();
            if (kv.second.random.is_open()) kv.second.random.close();
        }
        openFiles.clear();
    }

public:
    VirtualMachine() = default;
    explicit VirtualMachine(size_t stepLimit) : maxInstructionSteps(stepLimit) {}

    void setStepLimit(size_t stepLimit) {
        maxInstructionSteps = stepLimit;
    }

    void setStepHook(std::function<bool(size_t, const Instruction&)> hook) {
        stepHook = std::move(hook);
    }

    void run(std::vector<Instruction> bytecode, std::vector<Value> staticData, const std::vector<std::string>& symbols) {
        closeAllChannels();
        code = std::move(bytecode);
        dataValues = std::move(staticData);
        symbolNames = symbols;
        dataPtr = 0;
        ip = 0;
        stack.clear();
        callStack.clear();
        forLoopStack.clear();
        globalVariables.assign(symbolNames.size(), 0.0);
        arrays.clear();
        optionBase = 0;
        cursorRow = 1;
        cursorCol = 1;
        colorFg = 7;
        colorBg = 0;
        screenMode = 0;
        onErrorEnabled = false;
        onErrorTargetIp = -1;
        errorActive = false;
        errorResumeIp = -1;
        errorResumeNextIp = -1;
        lastErrorCode = 0;
        lastErrorLine = 0;
        printColumn = 0;
        size_t stepCount = 0;

        while (ip < code.size()) {
            const auto& instr = code[ip];

            try {
            ++stepCount;
            if (maxInstructionSteps > 0 && stepCount > maxInstructionSteps) {
                runtimeError("Execution limit exceeded", instr.line);
            }
            if (stepHook) {
                if (!stepHook(stepCount, instr)) {
                    runtimeError("Execution interrupted by step hook", instr.line);
                }
            }
            switch (instr.op) {
                case OpCode::PUSH_VAL: push(instr.operand); break;
                case OpCode::LOAD_VAR: {
                    int varIdx = static_cast<int>(asDouble(instr.operand));
                    push(getVariable(varIdx));
                    break;
                }
                case OpCode::STORE_VAR: {
                    int varIdx = static_cast<int>(asDouble(instr.operand));
                    setVariable(varIdx, pop());
                    break;
                }
                case OpCode::SET_OPTION_BASE: {
                    int base = static_cast<int>(asDouble(instr.operand));
                    if (base == 0 || base == 1) {
                        optionBase = base;
                    } else {
                        runtimeError("OPTION BASE only supports 0 or 1", instr.line);
                    }
                    break;
                }
                case OpCode::ERASE_ARRAY: {
                    int varIdx = static_cast<int>(asDouble(instr.operand));
                    arrays.erase(varIdx);
                    break;
                }
                case OpCode::SET_ON_ERROR: {
                    if (asDouble(instr.operand) == 0.0 || instr.targetIp < 0) {
                        onErrorEnabled = false;
                        onErrorTargetIp = -1;
                    } else {
                        onErrorEnabled = true;
                        onErrorTargetIp = instr.targetIp;
                    }
                    break;
                }
                case OpCode::RESUME_ERROR: {
                    if (!errorActive) {
                        runtimeError("RESUME without active error handler", instr.line);
                    }
                    int mode = static_cast<int>(std::lround(asDouble(instr.operand)));
                    int target = -1;
                    if (mode == 0) target = errorResumeIp;
                    else if (mode == 1) target = errorResumeNextIp;
                    else if (mode == 2) target = instr.targetIp;

                    if (target < 0 || target >= static_cast<int>(code.size())) {
                        runtimeError("RESUME target is invalid", instr.line);
                    }

                    errorActive = false;
                    unwindForLoopsForJump(target);
                    ip = static_cast<size_t>(target);
                    continue;
                }
                case OpCode::FILE_OPEN: {
                    int recLen = 128;
                    if (instr.targetIp == 3) {
                        recLen = asCheckedInt(pop(), instr.line, "Record length", 1, 1 << 20);
                        if (recLen <= 0) {
                            runtimeError("OPEN FOR RANDOM LEN must be positive", instr.line);
                        }
                    }
                    int channel = asCheckedPositiveChannel(pop(), instr.line, "OPEN");
                    std::string filename = valueToString(pop());
                    std::string sandboxedPath;
                    std::string sandboxError;
                    if (!resolveSandboxPath(filename, sandboxedPath, sandboxError)) {
                        runtimeError("OPEN sandbox violation: " + sandboxError, instr.line);
                    }
                    closeChannel(channel);

                    BasicFileHandle handle;
                    if (instr.targetIp == 0) {
                        handle.in.open(sandboxedPath);
                        handle.forInput = handle.in.is_open();
                        if (!handle.forInput) runtimeError("OPEN failed for INPUT: " + sandboxedPath, instr.line);
                    } else if (instr.targetIp == 1) {
                        handle.out.open(sandboxedPath, std::ios::out | std::ios::trunc);
                        handle.forOutput = handle.out.is_open();
                        if (!handle.forOutput) runtimeError("OPEN failed for OUTPUT: " + sandboxedPath, instr.line);
                    } else if (instr.targetIp == 2) {
                        handle.out.open(sandboxedPath, std::ios::out | std::ios::app);
                        handle.forOutput = handle.out.is_open();
                        if (!handle.forOutput) runtimeError("OPEN failed for APPEND: " + sandboxedPath, instr.line);
                    } else if (instr.targetIp == 3) {
                        handle.random.open(sandboxedPath, std::ios::in | std::ios::out | std::ios::binary);
                        if (!handle.random.is_open()) {
                            std::ofstream createFile(sandboxedPath, std::ios::out | std::ios::binary);
                            createFile.close();
                            handle.random.open(sandboxedPath, std::ios::in | std::ios::out | std::ios::binary);
                        }
                        handle.forRandom = handle.random.is_open();
                        handle.recordLen = recLen;
                        handle.currentRecord = 1;
                        if (!handle.forRandom) runtimeError("OPEN failed for RANDOM: " + sandboxedPath, instr.line);
                    } else {
                        runtimeError("Unsupported OPEN mode", instr.line);
                    }

                    if (handle.forInput || handle.forOutput || handle.forRandom) {
                        openFiles[channel] = std::move(handle);
                    }
                    break;
                }
                case OpCode::FILE_CLOSE: {
                    int channel = asCheckedPositiveChannel(pop(), instr.line, "CLOSE");
                    closeChannel(channel);
                    break;
                }
                case OpCode::FILE_CLOSE_ALL: {
                    closeAllChannels();
                    break;
                }
                case OpCode::FILE_PRINT: {
                    int itemCount = instr.targetIp;
                    int trailingSep = asCheckedInt(instr.operand, instr.line, "PRINT# separator", 0, 2);
                    std::vector<Value> items;
                    items.reserve(static_cast<size_t>(itemCount));
                    for (int i = 0; i < itemCount; ++i) {
                        items.push_back(pop());
                    }
                    std::reverse(items.begin(), items.end());
                    int channel = asCheckedPositiveChannel(pop(), instr.line, "PRINT#");

                    auto it = openFiles.find(channel);
                    if (it == openFiles.end() || !it->second.forOutput || !it->second.out.is_open()) {
                        runtimeError("PRINT# channel is not open for output", instr.line);
                        break;
                    }

                    for (int i = 0; i < itemCount; ++i) {
                        int sep = 0;
                        if (i > 0 && i < static_cast<int>(instr.targets.size())) {
                            sep = instr.targets[static_cast<size_t>(i)];
                        }
                        if (i > 0) {
                            if (sep == 2) it->second.out << ',';
                            else if (sep == 0) it->second.out << ' ';
                        }
                        it->second.out << valueToString(items[static_cast<size_t>(i)]);
                    }
                    if (trailingSep == 0) it->second.out << "\n";
                    break;
                }
                case OpCode::FILE_INPUT: {
                    int varCount = instr.targetIp;
                    int channel = asCheckedPositiveChannel(pop(), instr.line, "INPUT#");

                    auto it = openFiles.find(channel);
                    if (it == openFiles.end() || !it->second.forInput || !it->second.in.is_open()) {
                        runtimeError("INPUT# channel is not open for input", instr.line);
                        break;
                    }

                    std::string line;
                    if (!std::getline(it->second.in, line)) {
                        for (int i = 0; i < varCount && i < static_cast<int>(instr.targets.size()); ++i) {
                            setVariable(instr.targets[static_cast<size_t>(i)], "");
                        }
                        runtimeError("INPUT# reached end-of-file", instr.line);
                    }

                    auto fields = splitCsvSimple(line);
                    for (int i = 0; i < varCount && i < static_cast<int>(instr.targets.size()); ++i) {
                        CsvField field = (i < static_cast<int>(fields.size())) ? fields[static_cast<size_t>(i)] : CsvField{"", false};
                        assignInputFieldToVariable(instr.targets[static_cast<size_t>(i)], field, instr.line, "INPUT#");
                    }
                    break;
                }
                case OpCode::FILE_RANDOM_FIELD: {
                    int fieldCount = instr.targetIp;
                    if (fieldCount <= 0) {
                        runtimeError("FIELD requires at least one field binding", instr.line);
                    }
                    std::vector<int> lengths(fieldCount, 0);
                    for (int i = fieldCount - 1; i >= 0; --i) {
                        lengths[i] = asCheckedInt(pop(), instr.line, "FIELD length", 0, 1 << 20);
                    }
                    int channel = asCheckedPositiveChannel(pop(), instr.line, "FIELD");

                    auto it = openFiles.find(channel);
                    if (it == openFiles.end() || !it->second.forRandom || !it->second.random.is_open()) {
                        runtimeError("FIELD channel is not open for random access", instr.line);
                    }
                    if (static_cast<int>(instr.targets.size()) != fieldCount) {
                        runtimeError("FIELD metadata mismatch", instr.line);
                    }

                    it->second.fields.clear();
                    int offset = 0;
                    for (int i = 0; i < fieldCount; ++i) {
                        int len = lengths[i];
                        if (len < 0) len = 0;
                        BasicFileHandle::FieldBinding fb;
                        fb.varIdx = instr.targets[static_cast<size_t>(i)];
                        fb.length = len;
                        fb.offset = offset;
                        it->second.fields.push_back(fb);
                        offset += len;
                    }
                    if (offset > it->second.recordLen) {
                        runtimeError("FIELD total width exceeds OPEN LEN record size", instr.line);
                    }
                    break;
                }
                case OpCode::FILE_RANDOM_GET: {
                    int recordNum = -1;
                    if (asDouble(instr.operand) != 0.0) {
                        recordNum = asCheckedInt(pop(), instr.line, "GET record", 1, std::numeric_limits<int>::max());
                    }
                    int channel = asCheckedPositiveChannel(pop(), instr.line, "GET");

                    auto it = openFiles.find(channel);
                    if (it == openFiles.end() || !it->second.forRandom || !it->second.random.is_open()) {
                        runtimeError("GET channel is not open for random access", instr.line);
                    }
                    if (it->second.fields.empty()) {
                        runtimeError("GET requires FIELD bindings for this channel", instr.line);
                    }

                    if (recordNum <= 0) recordNum = static_cast<int>(it->second.currentRecord);
                    if (recordNum <= 0) recordNum = 1;
                    it->second.currentRecord = static_cast<long long>(recordNum);

                    long long offset = static_cast<long long>(recordNum - 1) * it->second.recordLen;
                    it->second.random.clear();
                    it->second.random.seekg(offset, std::ios::beg);
                    std::string buffer(static_cast<size_t>(it->second.recordLen), ' ');
                    it->second.random.read(&buffer[0], it->second.recordLen);
                    std::streamsize got = it->second.random.gcount();
                    if (got < it->second.recordLen) {
                        for (int i = static_cast<int>(got); i < it->second.recordLen; ++i) buffer[static_cast<size_t>(i)] = ' ';
                    }

                    for (const auto& fb : it->second.fields) {
                        if (fb.offset < 0 || fb.length < 0) continue;
                        if (fb.offset + fb.length > static_cast<int>(buffer.size())) continue;
                        std::string raw = buffer.substr(static_cast<size_t>(fb.offset), static_cast<size_t>(fb.length));
                        setVariable(fb.varIdx, trim(raw));
                    }
                    it->second.currentRecord = static_cast<long long>(recordNum) + 1;
                    break;
                }
                case OpCode::FILE_RANDOM_PUT: {
                    int recordNum = -1;
                    if (asDouble(instr.operand) != 0.0) {
                        recordNum = asCheckedInt(pop(), instr.line, "PUT record", 1, std::numeric_limits<int>::max());
                    }
                    int channel = asCheckedPositiveChannel(pop(), instr.line, "PUT");

                    auto it = openFiles.find(channel);
                    if (it == openFiles.end() || !it->second.forRandom || !it->second.random.is_open()) {
                        runtimeError("PUT channel is not open for random access", instr.line);
                    }
                    if (it->second.fields.empty()) {
                        runtimeError("PUT requires FIELD bindings for this channel", instr.line);
                    }

                    if (recordNum <= 0) recordNum = static_cast<int>(it->second.currentRecord);
                    if (recordNum <= 0) recordNum = 1;
                    it->second.currentRecord = static_cast<long long>(recordNum);

                    std::string buffer(static_cast<size_t>(it->second.recordLen), ' ');
                    for (const auto& fb : it->second.fields) {
                        if (fb.offset < 0 || fb.length <= 0) continue;
                        if (fb.offset + fb.length > static_cast<int>(buffer.size())) continue;
                        std::string value = valueToString(getVariable(fb.varIdx));
                        std::string fixed = toFixedWidth(value, fb.length);
                        for (int i = 0; i < fb.length; ++i) {
                            buffer[static_cast<size_t>(fb.offset + i)] = fixed[static_cast<size_t>(i)];
                        }
                    }

                    long long offset = static_cast<long long>(recordNum - 1) * it->second.recordLen;
                    it->second.random.clear();
                    it->second.random.seekp(offset, std::ios::beg);
                    it->second.random.write(buffer.data(), it->second.recordLen);
                    it->second.random.flush();
                    it->second.currentRecord = static_cast<long long>(recordNum) + 1;
                    break;
                }
                case OpCode::CMD_CLS: {
                    std::cout << "\x1B[2J\x1B[H";
                    printColumn = 0;
                    cursorRow = 1;
                    cursorCol = 1;
                    break;
                }
                case OpCode::CMD_LOCATE: {
                    int col = static_cast<int>(std::lround(asDouble(pop())));
                    int row = static_cast<int>(std::lround(asDouble(pop())));
                    if (row < 1) row = 1;
                    if (col < 1) col = 1;
                    cursorRow = row;
                    cursorCol = col;
                    printColumn = col - 1;
                    break;
                }
                case OpCode::CMD_COLOR: {
                    int bg = static_cast<int>(std::lround(asDouble(pop())));
                    int fg = static_cast<int>(std::lround(asDouble(pop())));
                    colorFg = fg;
                    if (bg >= 0) colorBg = bg;
                    break;
                }
                case OpCode::CMD_SCREEN: {
                    screenMode = static_cast<int>(std::lround(asDouble(pop())));
                    break;
                }
                case OpCode::CMD_PSET: {
                    int color = (instr.targetIp >= 3) ? static_cast<int>(std::lround(asDouble(pop()))) : colorFg;
                    int y = static_cast<int>(std::lround(asDouble(pop())));
                    int x = static_cast<int>(std::lround(asDouble(pop())));
                    int key = ((x & 0xFFFF) << 16) ^ (y & 0xFFFF);
                    virtualMemory[0x10000000 ^ key] = color;
                    break;
                }
                case OpCode::CMD_LINE: {
                    int color = (instr.targetIp >= 5) ? static_cast<int>(std::lround(asDouble(pop()))) : colorFg;
                    int y2 = static_cast<int>(std::lround(asDouble(pop())));
                    int x2 = static_cast<int>(std::lround(asDouble(pop())));
                    int y1 = static_cast<int>(std::lround(asDouble(pop())));
                    int x1 = static_cast<int>(std::lround(asDouble(pop())));
                    virtualMemory[0x20000001] = x1;
                    virtualMemory[0x20000002] = y1;
                    virtualMemory[0x20000003] = x2;
                    virtualMemory[0x20000004] = y2;
                    virtualMemory[0x20000005] = color;
                    break;
                }
                case OpCode::CMD_PLAY: {
                    std::string score = valueToString(pop());
                    virtualMemory[0x30000001] = static_cast<int>(score.size());
                    break;
                }
                case OpCode::CMD_SOUND: {
                    int duration = static_cast<int>(std::lround(asDouble(pop())));
                    int freq = static_cast<int>(std::lround(asDouble(pop())));
                    virtualMemory[0x30000002] = freq;
                    virtualMemory[0x30000003] = duration;
                    break;
                }
                case OpCode::DIM_ARRAY: {
                    int varIdx = static_cast<int>(asDouble(instr.operand));
                    int dims = instr.targetIp;
                    if (dims <= 0) {
                        runtimeError("DIM requires at least one dimension", instr.line);
                        break;
                    }
                    auto upperBounds = popArrayUpperBounds(dims);
                    bool ok = false;
                    ArrayData arr = makeArrayFromUpperBounds(upperBounds, instr.line, ok);
                    if (ok) arrays[varIdx] = std::move(arr);
                    break;
                }
                case OpCode::LOAD_ARRAY: {
                    int varIdx = static_cast<int>(asDouble(instr.operand));
                    int dims = instr.targetIp;
                    if (dims <= 0) {
                        runtimeError("Array reference requires at least one subscript", instr.line);
                        push(0.0);
                        break;
                    }
                    auto indices = popArrayIndices(dims);
                    if (!arrays.count(varIdx)) {
                        arrays[varIdx] = makeImplicitArray(dims, instr.line);
                    }
                    auto& arr = arrays[varIdx];
                    bool ok = false;
                    int linear = flattenArrayIndex(arr, indices, instr.line, ok);
                    if (ok) push(arr.data[linear]);
                    else push(0.0);
                    break;
                }
                case OpCode::STORE_ARRAY: {
                    int varIdx = static_cast<int>(asDouble(instr.operand));
                    int dims = instr.targetIp;
                    Value val = pop();
                    if (dims <= 0) {
                        runtimeError("Array assignment requires at least one subscript", instr.line);
                        break;
                    }
                    auto indices = popArrayIndices(dims);
                    if (!arrays.count(varIdx)) {
                        arrays[varIdx] = makeImplicitArray(dims, instr.line);
                    }

                    auto& arr = arrays[varIdx];
                    bool ok = false;
                    int linear = flattenArrayIndex(arr, indices, instr.line, ok);
                    if (ok) arr.data[linear] = val;
                    break;
                }
                case OpCode::ADD: {
                    Value b = pop(), a = pop();
                    if (std::holds_alternative<double>(a) && std::holds_alternative<double>(b)) {
                        push(std::get<double>(a) + std::get<double>(b));
                    } else if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
                        push(std::get<std::string>(a) + std::get<std::string>(b));
                    } else {
                        runtimeError("Type mismatch in '+' operation", instr.line);
                        push(0.0);
                    }
                    break;
                }
                case OpCode::SUB: {
                    Value b = pop(), a = pop();
                    if (requireNumericBinary(a, b, "-", instr.line)) push(std::get<double>(a) - std::get<double>(b));
                    else push(0.0);
                    break;
                }
                case OpCode::MUL: {
                    Value b = pop(), a = pop();
                    if (requireNumericBinary(a, b, "*", instr.line)) push(std::get<double>(a) * std::get<double>(b));
                    else push(0.0);
                    break;
                }
                case OpCode::POW: {
                    Value b = pop(), a = pop();
                    if (requireNumericBinary(a, b, "^", instr.line)) push(std::pow(std::get<double>(a), std::get<double>(b)));
                    else push(0.0);
                    break;
                }
                case OpCode::DIV: {
                    Value b = pop(), a = pop();
                    if (!requireNumericBinary(a, b, "/", instr.line)) { push(0.0); break; }
                    double divisor = std::get<double>(b);
                    if (divisor == 0.0) { runtimeError("Division by zero", instr.line); push(0.0); }
                    else push(std::get<double>(a) / divisor);
                    break;
                }
                case OpCode::INT_DIV: {
                    Value b = pop(), a = pop();
                    if (!requireNumericBinary(a, b, "\\", instr.line)) { push(0.0); break; }
                    double divisor = std::get<double>(b);
                    if (divisor == 0.0) { runtimeError("Integer division by zero", instr.line); push(0.0); }
                    else push(std::floor(std::get<double>(a) / divisor));
                    break;
                }
                case OpCode::MODULO: {
                    Value b = pop(), a = pop();
                    if (!requireNumericBinary(a, b, "MOD", instr.line)) { push(0.0); break; }
                    double divisor = std::get<double>(b);
                    if (divisor == 0.0) { runtimeError("Modulo by zero", instr.line); push(0.0); }
                    else push(std::fmod(std::get<double>(a), divisor));
                    break;
                }
                case OpCode::NEGATE: {
                    Value v = pop();
                    if (requireNumericUnary(v, "unary -", instr.line)) push(-std::get<double>(v));
                    else push(0.0);
                    break;
                }
                case OpCode::CMP_EQ: {
                    Value b = pop(), a = pop();
                    bool cmpResult = false;
                    compareValues(a, b, "=", instr.line, cmpResult);
                    push(cmpResult ? 1.0 : 0.0);
                    break;
                }
                case OpCode::CMP_LT: {
                    Value b = pop(), a = pop();
                    bool cmpResult = false;
                    compareValues(a, b, "<", instr.line, cmpResult);
                    push(cmpResult ? 1.0 : 0.0);
                    break;
                }
                case OpCode::CMP_GT: {
                    Value b = pop(), a = pop();
                    bool cmpResult = false;
                    compareValues(a, b, ">", instr.line, cmpResult);
                    push(cmpResult ? 1.0 : 0.0);
                    break;
                }
                case OpCode::CMP_LE: {
                    Value b = pop(), a = pop();
                    bool cmpResult = false;
                    compareValues(a, b, "<=", instr.line, cmpResult);
                    push(cmpResult ? 1.0 : 0.0);
                    break;
                }
                case OpCode::CMP_GE: {
                    Value b = pop(), a = pop();
                    bool cmpResult = false;
                    compareValues(a, b, ">=", instr.line, cmpResult);
                    push(cmpResult ? 1.0 : 0.0);
                    break;
                }
                case OpCode::CMP_NE: {
                    Value b = pop(), a = pop();
                    bool cmpResult = false;
                    compareValues(a, b, "<>", instr.line, cmpResult);
                    push(cmpResult ? 1.0 : 0.0);
                    break;
                }
                case OpCode::LOGICAL_AND: { Value b = pop(), a = pop(); push((valueToBool(a) && valueToBool(b)) ? 1.0 : 0.0); break; }
                case OpCode::LOGICAL_OR: { Value b = pop(), a = pop(); push((valueToBool(a) || valueToBool(b)) ? 1.0 : 0.0); break; }
                case OpCode::LOGICAL_NOT: push(!valueToBool(pop()) ? 1.0 : 0.0); break;
                case OpCode::PRINT_VAL: {
                    std::string out = valueToString(pop());
                    std::cout << out;
                    printColumn += static_cast<int>(out.size());
                    break;
                }
                case OpCode::PRINT_TAB: {
                    int nextStop = ((printColumn / 14) + 1) * 14;
                    int spaces = nextStop - printColumn;
                    if (spaces <= 0) spaces = 1;
                    printSpaces(spaces);
                    break;
                }
                case OpCode::PRINT_TAB_STOP: {
                    int targetCol = static_cast<int>(asDouble(pop()));
                    if (targetCol < 1) targetCol = 1;
                    int spaces = (targetCol - 1) - printColumn;
                    if (spaces > 0) printSpaces(spaces);
                    break;
                }
                case OpCode::PRINT_NEWLINE: std::cout << "\n"; printColumn = 0; break;
                case OpCode::INPUT: {
                    int mode = static_cast<int>(std::lround(asDouble(instr.operand)));
                    int varCount = instr.targetIp;

                    if (mode == 1 || mode == 2) {
                        std::string prompt = valueToString(pop());
                        std::cout << prompt;
                        if (mode == 2) {
                            std::cout << "? ";
                        }
                    } else {
                        std::cout << "? ";
                    }

                    std::string userInput;
                    std::getline(std::cin, userInput);
                    auto fields = splitCsvSimple(userInput);

                    for (int i = 0; i < varCount && i < static_cast<int>(instr.targets.size()); ++i) {
                        CsvField field = (i < static_cast<int>(fields.size())) ? fields[static_cast<size_t>(i)] : CsvField{"", false};
                        assignInputFieldToVariable(instr.targets[static_cast<size_t>(i)], field, instr.line, "INPUT");
                    }
                    break;
                }
                case OpCode::FOR_INIT: {
                    double step = asDouble(pop());
                    double limit = asDouble(pop());
                    int varIdx = static_cast<int>(asDouble(instr.operand));
                    double currentVal = asDouble(getVariable(varIdx));
                    forLoopStack.push_back({varIdx, limit, step, static_cast<int>(ip + 1), instr.targetIp});

                    bool runLoop = (step >= 0) ? (currentVal <= limit) : (currentVal >= limit);
                    if (!runLoop) {
                        forLoopStack.pop_back();
                        if (instr.targetIp >= 0) { ip = instr.targetIp; continue; }
                    }
                    break;
                }
                case OpCode::FOR_NEXT: {
                    if (!forLoopStack.empty()) {
                        auto& loop = forLoopStack.back();
                        double val = asDouble(getVariable(loop.varIdx)) + loop.step;
                        setVariable(loop.varIdx, val);

                        bool continueLoop = (loop.step >= 0) ? (val <= loop.limit) : (val >= loop.limit);
                        if (continueLoop) {
                            if (instr.targetIp >= 0) { ip = instr.targetIp; continue; }
                        } else {
                            forLoopStack.pop_back();
                        }
                    }
                    break;
                }
                case OpCode::READ_VAR: {
                    int varIdx = static_cast<int>(asDouble(instr.operand));
                    if (dataPtr < dataValues.size()) {
                        setVariable(varIdx, dataValues[dataPtr++]);
                    } else {
                        runtimeError("Out of DATA", instr.line);
                    }
                    break;
                }
                case OpCode::RESTORE_DATA: dataPtr = 0; break;
                case OpCode::POKE_MEM: {
                    int value = static_cast<int>(std::lround(asDouble(pop())));
                    int address = static_cast<int>(std::lround(asDouble(pop())));
                    if (address < 0) {
                        runtimeError("POKE address must be non-negative", instr.line);
                    } else {
                        value %= 256;
                        if (value < 0) value += 256;
                        virtualMemory[address] = value;
                    }
                    break;
                }
                case OpCode::JUMP:
                    if (instr.targetIp >= 0) {
                        unwindForLoopsForJump(instr.targetIp);
                        ip = instr.targetIp;
                        continue;
                    }
                    break;
                case OpCode::JUMP_IF_FALSE:
                    if (!valueToBool(pop())) {
                        if (instr.targetIp >= 0) {
                            unwindForLoopsForJump(instr.targetIp);
                            ip = instr.targetIp;
                            continue;
                        }
                    }
                    break;
                case OpCode::ON_GOTO: {
                    int idx = static_cast<int>(asDouble(pop())) - 1;
                    if (idx >= 0 && idx < static_cast<int>(instr.targets.size())) {
                        unwindForLoopsForJump(instr.targets[idx]);
                        ip = instr.targets[idx];
                        continue;
                    }
                    break;
                }
                case OpCode::ON_GOSUB: {
                    int idx = static_cast<int>(asDouble(pop())) - 1;
                    if (idx >= 0 && idx < static_cast<int>(instr.targets.size())) {
                        unwindForLoopsForJump(instr.targets[idx]);
                        callStack.push_back(ip + 1);
                        ip = instr.targets[idx];
                        continue;
                    }
                    break;
                }
                case OpCode::CALL:
                    callStack.push_back(ip + 1);
                    if (instr.targetIp >= 0) {
                        unwindForLoopsForJump(instr.targetIp);
                        ip = instr.targetIp;
                        continue;
                    }
                    break;
                case OpCode::RETURN:
                    if (!callStack.empty()) {
                        int returnIp = static_cast<int>(callStack.back());
                        callStack.pop_back();
                        unwindForLoopsForJump(returnIp);
                        ip = static_cast<size_t>(returnIp);
                        continue;
                    }
                    break;
                case OpCode::FN_SQRT: push(std::sqrt(asDouble(pop()))); break;
                case OpCode::FN_ABS: push(std::abs(asDouble(pop()))); break;
                case OpCode::FN_RND: {
                    double n = asDouble(pop());
                    if (n < 0.0) {
                        std::srand(static_cast<unsigned int>(std::llabs(static_cast<long long>(std::llround(n)))));
                        lastRndValue = static_cast<double>(rand()) / RAND_MAX;
                    } else if (n > 0.0) {
                        lastRndValue = static_cast<double>(rand()) / RAND_MAX;
                    }
                    push(lastRndValue);
                    break;
                }
                case OpCode::FN_INT: push(std::floor(asDouble(pop()))); break;
                case OpCode::FN_LEN: push(static_cast<double>(valueToString(pop()).length())); break;
                case OpCode::FN_POS: push(static_cast<double>(printColumn + 1)); break;
                case OpCode::FN_PEEK: {
                    int address = static_cast<int>(std::lround(asDouble(pop())));
                    if (address < 0) {
                        runtimeError("PEEK address must be non-negative", instr.line);
                        push(0.0);
                    } else {
                        auto it = virtualMemory.find(address);
                        push(static_cast<double>((it != virtualMemory.end()) ? it->second : 0));
                    }
                    break;
                }
                case OpCode::FN_USR: {
                    int address = static_cast<int>(std::lround(asDouble(pop())));
                    if (address < 0) {
                        runtimeError("USR address must be non-negative", instr.line);
                        push(0.0);
                    } else {
                        auto it = virtualMemory.find(address);
                        push(static_cast<double>((it != virtualMemory.end()) ? it->second : 0));
                    }
                    break;
                }
                case OpCode::FN_INKEY: {
                    push(std::string(""));
                    break;
                }
                case OpCode::FN_ERR: {
                    push(static_cast<double>(lastErrorCode));
                    break;
                }
                case OpCode::FN_ERL: {
                    push(static_cast<double>(lastErrorLine));
                    break;
                }
                case OpCode::FN_EOF: {
                    int channel = asCheckedPositiveChannel(pop(), instr.line, "EOF");
                    auto it = openFiles.find(channel);
                    if (it == openFiles.end()) {
                        runtimeError("EOF channel is not open", instr.line);
                    }

                    if (it->second.forInput && it->second.in.is_open()) {
                        int c = it->second.in.peek();
                        push(c == std::char_traits<char>::eof() ? 1.0 : 0.0);
                    } else if (it->second.forRandom && it->second.random.is_open()) {
                        std::streampos saved = it->second.random.tellg();
                        it->second.random.clear();
                        it->second.random.seekg(0, std::ios::end);
                        std::streamoff endPos = it->second.random.tellg();
                        if (saved != std::streampos(-1)) {
                            it->second.random.seekg(saved, std::ios::beg);
                        }
                        long long nextOffset = static_cast<long long>(it->second.currentRecord - 1) * it->second.recordLen;
                        push((endPos >= 0 && nextOffset >= static_cast<long long>(endPos)) ? 1.0 : 0.0);
                    } else {
                        push(0.0);
                    }
                    break;
                }
                case OpCode::FN_LEFT: {
                    int n = static_cast<int>(asDouble(pop()));
                    std::string str = valueToString(pop());
                    if (n <= 0) push("");
                    else if (n >= static_cast<int>(str.length())) push(str);
                    else push(str.substr(0, n));
                    break;
                }
                case OpCode::FN_RIGHT: {
                    int n = static_cast<int>(asDouble(pop()));
                    std::string str = valueToString(pop());
                    if (n <= 0) push("");
                    else if (n >= static_cast<int>(str.length())) push(str);
                    else push(str.substr(str.length() - n));
                    break;
                }
                case OpCode::FN_MID: {
                    int len = static_cast<int>(asDouble(pop()));
                    int start = static_cast<int>(asDouble(pop())) - 1;
                    std::string str = valueToString(pop());
                    if (start < 0) start = 0;
                    if (start >= static_cast<int>(str.length())) { push(""); break; }
                    if (len < 0 || start + len > static_cast<int>(str.length())) push(str.substr(start));
                    else push(str.substr(start, len));
                    break;
                }
                case OpCode::FN_CHR: {
                    int codeVal = static_cast<int>(asDouble(pop()));
                    push(std::string(1, static_cast<char>(codeVal)));
                    break;
                }
                case OpCode::FN_ASC: {
                    std::string str = valueToString(pop());
                    push(!str.empty() ? static_cast<double>(static_cast<unsigned char>(str[0])) : 0.0);
                    break;
                }
                case OpCode::FN_VAL: {
                    push(asDouble(pop()));
                    break;
                }
                case OpCode::FN_STR: {
                    push(valueToString(pop()));
                    break;
                }
                case OpCode::SHOW_HELP: printHelp(); break;
                case OpCode::SHOW_VERSION: printVersion(); break;
                case OpCode::HALT: return;
            }
            } catch (const RuntimeErrorSignal& err) {
                int line = err.line > 0 ? err.line : instr.line;
                lastErrorCode = classifyErrorCode(err.message);
                lastErrorLine = line;

                if (onErrorEnabled && onErrorTargetIp >= 0 && !errorActive) {
                    errorActive = true;
                    errorResumeIp = static_cast<int>(ip);
                    errorResumeNextIp = static_cast<int>(ip + 1);
                    unwindForLoopsForJump(onErrorTargetIp);
                    ip = static_cast<size_t>(onErrorTargetIp);
                    continue;
                }

                std::string message = "Runtime Error";
                if (line > 0) message += " on line " + std::to_string(line);
                message += ": " + err.message;
                printErrorLine(message);
                return;
            }
            ip++;
        }
    }
};

// ============================================================================
// 5. CLI HANDLER & REPL ENVIRONMENT
// ============================================================================
void executeSource(const std::string& source, bool dump = false) {
    std::stringstream ss(source);
    std::map<int, std::string> lineMap;
    std::string line;

    struct PendingUnnumberedLine {
        std::string text;
    };
    std::vector<PendingUnnumberedLine> unnumberedLines;

    while (std::getline(ss, line)) {
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        std::string trimmed = line.substr(first);
        if (std::isdigit(trimmed[0])) {
            int lineNum = std::stoi(trimmed);
            lineMap[lineNum] = trimmed;
        } else {
            unnumberedLines.push_back({trimmed});
        }
    }

    int autoLine = lineMap.empty() ? 10 : (lineMap.rbegin()->first + 10);
    for (const auto& pending : unnumberedLines) {
        while (lineMap.count(autoLine) != 0) {
            autoLine += 10;
        }
        lineMap[autoLine] = std::to_string(autoLine) + " " + pending.text;
        autoLine += 10;
    }

    std::stringstream sortedSource;
    for (const auto& entry : lineMap) sortedSource << entry.second << "\n";

    auto tokens = tokenizeSource(sortedSource.str());

    Compiler compiler(tokens);
    auto [bytecode, dataValues, symbolTable] = compiler.compile();

    if (dump) dumpBytecode(bytecode, dataValues, symbolTable);

    VirtualMachine vm;
    vm.run(bytecode, dataValues, symbolTable);
}

void runRepl() {
    std::cout << "CrossBASIC Interpreter & Virtual Machine v3.0\n";
    std::cout << "Type BASIC statements, 'VERSION', 'SAVE', 'LOAD', 'HELP', or 'EXIT' to quit.\n\n";

    VirtualMachine vm;
    std::map<int, std::string> programMemory;

    std::string line;
    while (true) {
        std::cout << "BASIC> ";
        if (!std::getline(std::cin, line)) break;

        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        size_t last = line.find_last_not_of(" \t\r\n");
        std::string trimmed = line.substr(first, (last - first + 1));

        std::string upperLine = trimmed;
        std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::toupper);

        if (upperLine == "EXIT") break;

        // Line number entry
        if (std::isdigit(trimmed[0])) {
            std::stringstream lss(trimmed);
            int lineNum;
            lss >> lineNum;
            
            std::string restOfLine;
            std::getline(lss, restOfLine);
            size_t rFirst = restOfLine.find_first_not_of(" \t\r\n");
            
            if (rFirst == std::string::npos) {
                programMemory.erase(lineNum);
                std::cout << "Line " << lineNum << " removed.\n";
            } else {
                programMemory[lineNum] = trimmed;
            }
            continue;
        }

        // Direct Commands
        std::stringstream css(trimmed);
        std::string cmd;
        css >> cmd;
        std::string upperCmd = cmd;
        std::transform(upperCmd.begin(), upperCmd.end(), upperCmd.begin(), ::toupper);

        if (upperCmd == "VERSION") { printVersion(); continue; }
        else if (upperCmd == "RUN") {
            if (programMemory.empty()) { std::cout << "No program stored in memory.\n"; continue; }
            std::stringstream fullSource;
            for (const auto& pair : programMemory) fullSource << pair.second << "\n";
            
            auto tokens = tokenizeSource(fullSource.str());
            Compiler compiler(tokens);
            auto [bytecode, dataValues, symbolTable] = compiler.compile();
            vm.run(bytecode, dataValues, symbolTable);
            continue;
        }
        else if (upperCmd == "LIST") {
            if (programMemory.empty()) std::cout << "Program memory is empty.\n";
            else for (const auto& pair : programMemory) std::cout << pair.second << "\n";
            continue;
        }
        else if (upperCmd == "NEW") {
            programMemory.clear();
            vm = VirtualMachine();
            std::cout << "Program memory cleared.\n";
            continue;
        }
        else if (upperCmd == "SAVE") {
            std::string arg;
            std::getline(css, arg);
            std::string filename = ensureExtension(arg);

            if (filename.empty()) { printErrorLine("Error: Specify filename."); continue; }
            std::string sandboxedPath;
            std::string sandboxError;
            if (!resolveSandboxPath(filename, sandboxedPath, sandboxError)) {
                printErrorLine("Error: SAVE sandbox violation: " + sandboxError);
                continue;
            }

            std::ofstream outFile(sandboxedPath);
            if (!outFile.is_open()) { printErrorLine("Error: Could not open file " + sandboxedPath); continue; }
            for (const auto& pair : programMemory) outFile << pair.second << "\n";
            outFile.close();
            std::cout << "Program saved to " << sandboxedPath << "\n";
            continue;
        }
        else if (upperCmd == "LOAD") {
            std::string arg;
            std::getline(css, arg);
            std::string filename = ensureExtension(arg);

            if (filename.empty()) { printErrorLine("Error: Specify filename."); continue; }
            std::string sandboxedPath;
            std::string sandboxError;
            if (!resolveSandboxPath(filename, sandboxedPath, sandboxError)) {
                printErrorLine("Error: LOAD sandbox violation: " + sandboxError);
                continue;
            }

            std::ifstream inFile(sandboxedPath);
            if (!inFile.is_open()) { printErrorLine("Error: Could not open file " + sandboxedPath); continue; }
            programMemory.clear();
            std::string fileLine;
            int lineCount = 0, autoLineNum = 10;
            std::vector<std::string> unnumberedLoadedLines;

            while (std::getline(inFile, fileLine)) {
                size_t fFirst = fileLine.find_first_not_of(" \t\r\n");
                if (fFirst == std::string::npos) continue;
                std::string fTrimmed = fileLine.substr(fFirst);

                if (std::isdigit(fTrimmed[0])) {
                    std::stringstream lss(fTrimmed);
                    int lNum; lss >> lNum;
                    programMemory[lNum] = fTrimmed;
                } else {
                    unnumberedLoadedLines.push_back(fTrimmed);
                }
                lineCount++;
            }

            autoLineNum = programMemory.empty() ? 10 : (programMemory.rbegin()->first + 10);
            for (const auto& pending : unnumberedLoadedLines) {
                while (programMemory.count(autoLineNum) != 0) {
                    autoLineNum += 10;
                }
                programMemory[autoLineNum] = std::to_string(autoLineNum) + " " + pending;
                autoLineNum += 10;
            }

            inFile.close();
            std::cout << "Program loaded from " << sandboxedPath << " (" << lineCount << " lines).\n";
            continue;
        }

        // Execute immediate statement
        auto tokens = tokenizeSource(trimmed);
        Compiler compiler(tokens);
        auto [bytecode, dataValues, symbolTable] = compiler.compile();
        vm.run(bytecode, dataValues, symbolTable);
    }
}

int main(int argc, char* argv[]) {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    std::error_code ec;
    g_virtualRoot = fs::current_path(ec);
    if (ec) {
        printErrorLine("Error: Unable to resolve sandbox root directory.");
        return 1;
    }

    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "--help" || arg1 == "-h" || arg1 == "/?") { printHelp(); return 0; }
        if (arg1 == "--version" || arg1 == "-v") { printVersion(); return 0; }

        bool dump = false;
        if (argc > 2 && std::string(argv[2]) == "--dump") dump = true;

        std::string filename = ensureExtension(arg1);
        std::string sandboxedPath;
        std::string sandboxError;
        if (!resolveSandboxPath(filename, sandboxedPath, sandboxError)) {
            printErrorLine("Error: LOAD sandbox violation: " + sandboxError);
            return 1;
        }

        std::ifstream file(sandboxedPath);
        if (!file.is_open()) {
            printErrorLine("Error: Could not open file " + sandboxedPath);
            return 1;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        executeSource(buffer.str(), dump);
    } else {
        runRepl();
    }
    return 0;
}
