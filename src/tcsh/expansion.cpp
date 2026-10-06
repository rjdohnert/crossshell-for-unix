#include "engine.hpp"
#include "expansion.hpp"

std::string TcshEngine::joinPositionalArgs() const {
    std::string joined;
    for (size_t i = 0; i < positionalArgs.size(); ++i) {
        if (i > 0) joined += " ";
        joined += positionalArgs[i];
    }
    return joined;
}

void TcshEngine::setVariableList(const std::string& key, const std::vector<std::string>& val) {
    variables[key] = val;
    if (key == "prompt" && !val.empty()) promptStr = val[0];

    // Synchronize 'path' array with Windows 'PATH' environment variable
    if (key == "path") {
        std::string pathEnv = "";
        for (size_t i = 0; i < val.size(); ++i) {
            if (i > 0) pathEnv += ";";
            pathEnv += val[i];
        }
        SetEnvironmentVariableW(L"PATH", string_to_wstring(pathEnv).c_str());
    }
}

std::string TcshEngine::getVariableString(const std::string& key) const {
    auto it = variables.find(key);
    if (it != variables.end()) {
        std::string res;
        for (size_t i = 0; i < it->second.size(); ++i) {
            if (i > 0) res += " ";
            res += it->second[i];
        }
        return res;
    }
    return "";
}

bool TcshEngine::isVariableSet(const std::string& token) const {
    if (token.empty()) return false;
    if (token == "argv" || token == "#argv" || token == "0") return !scriptName.empty();
    if (variables.count(token) > 0) return true;

    std::wstring wToken = string_to_wstring(token);
    SetLastError(ERROR_SUCCESS);
    DWORD envLen = GetEnvironmentVariableW(wToken.c_str(), nullptr, 0);
    return (envLen > 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND);
}

void TcshEngine::setScriptArguments(const std::string& filename, const std::vector<std::string>& args) {
    scriptName = filename;
    positionalArgs = args;
}

bool TcshEngine::shiftPositionalArguments(size_t count) {
    if (count > positionalArgs.size()) return false;
    positionalArgs.erase(positionalArgs.begin(), positionalArgs.begin() + static_cast<std::ptrdiff_t>(count));
    return true;
}

std::string TcshEngine::applyModifier(const std::string& str, char mod) const {
    if (str.empty()) return str;
    if (mod == 'h') { // Head
        size_t p = str.find_last_of("/\\");
        return (p != std::string::npos) ? str.substr(0, p) : "";
    }
    if (mod == 't') { // Tail
        size_t p = str.find_last_of("/\\");
        return (p != std::string::npos) ? str.substr(p + 1) : str;
    }
    if (mod == 'r') { // Root
        size_t p = str.find_last_of('.');
        return (p != std::string::npos) ? str.substr(0, p) : str;
    }
    if (mod == 'e') { // Extension
        size_t p = str.find_last_of('.');
        return (p != std::string::npos) ? str.substr(p + 1) : "";
    }
    if (mod == 'u') { // Upper
        std::string res = str;
        std::transform(res.begin(), res.end(), res.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return res;
    }
    if (mod == 'l') { // Lower
        std::string res = str;
        std::transform(res.begin(), res.end(), res.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return res;
    }
    return str;
}

std::string TcshEngine::expandVariables(const std::string& str) {
    std::string res;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;

    for (size_t i = 0; i < str.length(); ++i) {
        char c = str[i];
        if (c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            res += c;
        } else if (c == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            res += c;
        } else if (c == '~' && !inSingleQuote && (i == 0 || std::isspace(static_cast<unsigned char>(str[i - 1])))) {
            if (i + 1 == str.length() || str[i + 1] == '/' || str[i + 1] == '\\') {
                res += homeDir;
            } else {
                res += c;
            }
        } else if (c == '$' && !inSingleQuote) {
            size_t varStart = i + 1;
            bool checkIsSet = false;
            bool checkCount = false;
            bool braceMode = false;

            if (varStart < str.length() && str[varStart] == '?') {
                checkIsSet = true;
                varStart++;
            } else if (varStart < str.length() && str[varStart] == '#') {
                checkCount = true;
                varStart++;
            } else if (varStart < str.length() && str[varStart] == '{') {
                braceMode = true;
                varStart++;
            }

            size_t varLen = 0;
            size_t braceEnd = std::string::npos;
            if (braceMode) {
                braceEnd = str.find('}', varStart);
                if (braceEnd != std::string::npos) {
                    varLen = braceEnd - varStart;
                }
            } else {
                while (varStart + varLen < str.length() &&
                      (std::isalnum(static_cast<unsigned char>(str[varStart + varLen])) || str[varStart + varLen] == '_')) {
                    varLen++;
                }
            }

            int index = -1;
            if (!braceMode && varStart + varLen < str.length() && str[varStart + varLen] == '[') {
                size_t closeBracket = str.find(']', varStart + varLen + 1);
                if (closeBracket != std::string::npos) {
                    std::string idxStr = str.substr(varStart + varLen + 1, closeBracket - (varStart + varLen + 1));
                    index = std::atoi(idxStr.c_str());
                    varLen = (closeBracket - varStart) + 1;
                }
            }

            std::string varName = str.substr(varStart, varLen);
            std::vector<char> modifiers;
            size_t modScan = varStart + varLen;
            while (modScan < str.length() && str[modScan] == ':') {
                if (modScan + 1 < str.length()) {
                    modifiers.push_back(str[modScan + 1]);
                    modScan += 2;
                } else break;
            }

            if (!varName.empty()) {
                std::string valStr;
                if (checkIsSet) {
                    valStr = isVariableSet(varName) ? "1" : "0";
                } else if (checkCount) {
                    if (variables.count(varName)) valStr = std::to_string(variables[varName].size());
                    else if (varName == "argv") valStr = std::to_string(positionalArgs.size());
                    else valStr = "0";
                } else {
                    if (variables.count(varName)) {
                        const auto& list = variables[varName];
                        if (index > 0 && static_cast<size_t>(index) <= list.size()) valStr = list[index - 1];
                        else valStr = getVariableString(varName);
                    } else if (varName == "argv") {
                        if (index > 0 && static_cast<size_t>(index) <= positionalArgs.size()) valStr = positionalArgs[index - 1];
                        else valStr = joinPositionalArgs();
                    } else {
                        std::wstring wVarName = string_to_wstring(varName);
                        DWORD dwRet = GetEnvironmentVariableW(wVarName.c_str(), nullptr, 0);
                        if (dwRet > 0) {
                            std::vector<wchar_t> envBuf(dwRet);
                            if (GetEnvironmentVariableW(wVarName.c_str(), envBuf.data(), dwRet) > 0) {
                                valStr = wstring_to_string(envBuf.data());
                            }
                        }
                    }
                }

                for (char mod : modifiers) valStr = applyModifier(valStr, mod);
                res += valStr;
                if (braceMode && braceEnd != std::string::npos) {
                    i = braceEnd;
                } else {
                    i = modScan - 1;
                }
            } else {
                res += '$';
            }
        } else {
            res += c;
        }
    }
    return res;
}


std::string TcshEngine::expandHistory(const std::string& line) {
    if (line.empty() || line[0] != '!') return line;
    if (history.empty()) return line;

    if (line == "!!" || line == "!*") return history.back();
    if (line == "!$") {
        std::vector<std::string> tokens = tokenize(history.back(), ' ');
        return tokens.empty() ? "" : tokens.back();
    }
    if (line.size() > 1 && std::isdigit(line[1])) {
        int idx = std::atoi(line.substr(1).c_str());
        if (idx > 0 && static_cast<size_t>(idx) <= history.size()) return history[idx - 1];
    }
    return line;
}


std::vector<std::string> TcshEngine::expandGlobs(const std::vector<std::string>& args) const {
    std::vector<std::string> expanded;
    for (const auto& arg : args) {
        if (arg.find('*') != std::string::npos || arg.find('?') != std::string::npos) {
            std::wstring wPattern = string_to_wstring(arg);
            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW(wPattern.c_str(), &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    expanded.push_back(wstring_to_string(fd.cFileName));
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            } else {
                expanded.push_back(arg);
            }
        } else {
            expanded.push_back(arg);
        }
    }
    return expanded;
}

void TcshEngine::evaluateArithmetic(const std::string& expr) {
    std::vector<std::string> tokens = tokenize(expr, ' ');
    if (tokens.size() < 3 || tokens[1] != "=") {
        return;
    }

    std::string varName = stripOuterQuotes(tokens[0]);
    std::string rhs = expr;
    size_t eqPos = rhs.find('=');
    if (eqPos == std::string::npos) {
        return;
    }
    rhs = rhs.substr(eqPos + 1);

    auto trimInPlace = [](std::string& value) {
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
            value.pop_back();
        }
    };

    auto resolveNumericToken = [&](const std::string& token, long long& outValue) -> bool {
        std::string cleaned = stripOuterQuotes(token);
        if (cleaned.empty()) return false;

        auto varIt = variables.find(cleaned);
        if (varIt != variables.end() && !varIt->second.empty()) {
            cleaned = stripOuterQuotes(varIt->second[0]);
        }

        char* endPtr = nullptr;
        errno = 0;
        long long parsed = std::strtoll(cleaned.c_str(), &endPtr, 10);
        if (errno != 0 || endPtr == cleaned.c_str() || (endPtr != nullptr && *endPtr != '\0')) {
            return false;
        }
        outValue = parsed;
        return true;
    };

    auto precedence = [](char op) -> int {
        if (op == '+' || op == '-') return 1;
        if (op == '*' || op == '/' || op == '%') return 2;
        return 0;
    };

    auto applyOp = [](long long lhs, long long rhsValue, char op, long long& outValue) -> bool {
        switch (op) {
            case '+': outValue = lhs + rhsValue; return true;
            case '-': outValue = lhs - rhsValue; return true;
            case '*': outValue = lhs * rhsValue; return true;
            case '/':
                if (rhsValue == 0) return false;
                outValue = lhs / rhsValue;
                return true;
            case '%':
                if (rhsValue == 0) return false;
                outValue = lhs % rhsValue;
                return true;
            default:
                return false;
        }
    };

    trimInPlace(rhs);
    std::vector<std::string> outputQueue;
    std::vector<char> opStack;

    auto popOperators = [&](char untilOp) {
        while (!opStack.empty() && opStack.back() != untilOp) {
            outputQueue.push_back(std::string(1, opStack.back()));
            opStack.pop_back();
        }
    };

    for (size_t i = 0; i < rhs.size();) {
        char c = rhs[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }

        bool unaryMinus = false;
        if (c == '-') {
            size_t j = i;
            while (j > 0 && std::isspace(static_cast<unsigned char>(rhs[j - 1]))) {
                --j;
            }
            if (j == 0 || rhs[j - 1] == '(' || rhs[j - 1] == '+' || rhs[j - 1] == '-' || rhs[j - 1] == '*' || rhs[j - 1] == '/' || rhs[j - 1] == '%') {
                unaryMinus = true;
            }
        }

        if (std::isdigit(static_cast<unsigned char>(c)) || unaryMinus) {
            size_t start = i;
            if (unaryMinus) {
                ++i;
            }
            while (i < rhs.size() && std::isdigit(static_cast<unsigned char>(rhs[i]))) {
                ++i;
            }
            if (i == start + (unaryMinus ? 1 : 0)) {
                return;
            }
            outputQueue.push_back(rhs.substr(start, i - start));
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = i;
            ++i;
            while (i < rhs.size() && (std::isalnum(static_cast<unsigned char>(rhs[i])) || rhs[i] == '_')) {
                ++i;
            }
            outputQueue.push_back(rhs.substr(start, i - start));
            continue;
        }

        if (c == '(') {
            opStack.push_back(c);
            ++i;
            continue;
        }

        if (c == ')') {
            popOperators('(');
            if (opStack.empty() || opStack.back() != '(') {
                return;
            }
            opStack.pop_back();
            ++i;
            continue;
        }

        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') {
            while (!opStack.empty() && opStack.back() != '(' && precedence(opStack.back()) >= precedence(c)) {
                outputQueue.push_back(std::string(1, opStack.back()));
                opStack.pop_back();
            }
            opStack.push_back(c);
            ++i;
            continue;
        }

        return;
    }

    while (!opStack.empty()) {
        if (opStack.back() == '(' || opStack.back() == ')') {
            return;
        }
        outputQueue.push_back(std::string(1, opStack.back()));
        opStack.pop_back();
    }

    std::vector<long long> valueStack;
    for (const auto& token : outputQueue) {
        if (token.size() == 1 && (token[0] == '+' || token[0] == '-' || token[0] == '*' || token[0] == '/' || token[0] == '%')) {
            if (valueStack.size() < 2) {
                return;
            }
            long long rhsValue = valueStack.back();
            valueStack.pop_back();
            long long lhs = valueStack.back();
            valueStack.pop_back();
            long long result = 0;
            if (!applyOp(lhs, rhsValue, token[0], result)) {
                return;
            }
            valueStack.push_back(result);
            continue;
        }

        long long value = 0;
        if (!resolveNumericToken(token, value)) {
            return;
        }
        valueStack.push_back(value);
    }

    if (valueStack.size() == 1) {
        setVariableList(varName, { std::to_string(valueStack.back()) });
    }
}

bool TcshEngine::evalCondition(const std::string& expr) {
    std::string cleanExpr = expr;
    cleanExpr.erase(cleanExpr.begin(), std::find_if(cleanExpr.begin(), cleanExpr.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    while (!cleanExpr.empty() && std::isspace(static_cast<unsigned char>(cleanExpr.back()))) {
        cleanExpr.pop_back();
    }

    // Outer paren stripping
    if (cleanExpr.size() >= 2 && cleanExpr.front() == '(' && cleanExpr.back() == ')') {
        int parenDepth = 0;
        bool matchesOuter = true;
        for (size_t i = 0; i < cleanExpr.size(); ++i) {
            if (cleanExpr[i] == '(') parenDepth++;
            else if (cleanExpr[i] == ')') parenDepth--;
            if (parenDepth == 0 && i < cleanExpr.size() - 1) {
                matchesOuter = false;
                break;
            }
        }
        if (matchesOuter) {
            return evalCondition(cleanExpr.substr(1, cleanExpr.size() - 2));
        }
    }

    // Quote and paren aware search for logical operators || and &&
    bool inDoubleQuote = false;
    bool inSingleQuote = false;
    int depth = 0;

    for (size_t i = 0; i < cleanExpr.size(); ++i) {
        char c = cleanExpr[i];
        if (c == '"' && !inSingleQuote) inDoubleQuote = !inDoubleQuote;
        else if (c == '\'' && !inDoubleQuote) inSingleQuote = !inSingleQuote;
        else if (!inDoubleQuote && !inSingleQuote) {
            if (c == '(') depth++;
            else if (c == ')') depth--;
            else if (depth == 0) {
                if (i + 1 < cleanExpr.size() && cleanExpr[i] == '|' && cleanExpr[i + 1] == '|') {
                    return evalCondition(cleanExpr.substr(0, i)) || evalCondition(cleanExpr.substr(i + 2));
                }
            }
        }
    }

    inDoubleQuote = false;
    inSingleQuote = false;
    depth = 0;
    for (size_t i = 0; i < cleanExpr.size(); ++i) {
        char c = cleanExpr[i];
        if (c == '"' && !inSingleQuote) inDoubleQuote = !inDoubleQuote;
        else if (c == '\'' && !inDoubleQuote) inSingleQuote = !inSingleQuote;
        else if (!inDoubleQuote && !inSingleQuote) {
            if (c == '(') depth++;
            else if (c == ')') depth--;
            else if (depth == 0) {
                if (i + 1 < cleanExpr.size() && cleanExpr[i] == '&' && cleanExpr[i + 1] == '&') {
                    return evalCondition(cleanExpr.substr(0, i)) && evalCondition(cleanExpr.substr(i + 2));
                }
            }
        }
    }

    std::vector<std::string> tokens = tokenize(cleanExpr, ' ');
    if (tokens.empty()) return false;

    if (tokens.size() >= 2 && tokens[0][0] == '-') {
        std::string op = tokens[0];
        std::string target = stripOuterQuotes(tokens[1]);
        std::wstring wTarget = string_to_wstring(target);
        DWORD attr = GetFileAttributesW(wTarget.c_str());

        if (op == "-e") return (attr != INVALID_FILE_ATTRIBUTES);
        if (op == "-d") return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
        if (op == "-f") return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
        if (op == "-r") return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_READONLY));
        if (op == "-w") return (attr != INVALID_FILE_ATTRIBUTES);
        if (op == "-x") {
            if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) return false;
            std::string lowerTarget = target;
            std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return (lowerTarget.find(".exe") != std::string::npos || lowerTarget.find(".bat") != std::string::npos ||
                    lowerTarget.find(".cmd") != std::string::npos || lowerTarget.find(".com") != std::string::npos ||
                    lowerTarget.find(".ps1") != std::string::npos);
        }
        if (op == "-z") {
            WIN32_FILE_ATTRIBUTE_DATA data;
            if (GetFileAttributesExW(wTarget.c_str(), GetFileExInfoStandard, &data)) {
                return (data.nFileSizeHigh == 0 && data.nFileSizeLow == 0);
            }
            return false;
        }
    }

    if (tokens.size() >= 3) {
        std::string left = stripOuterQuotes(tokens[0]);
        std::string op = tokens[1];
        std::string right = stripOuterQuotes(tokens[2]);

        if (op == "==") return left == right;
        if (op == "!=") return left != right;
        if (op == "=~") return wildcard_match(right, left);
        if (op == "!~") return !wildcard_match(right, left);
        if (op == ">") return std::atoll(left.c_str()) > std::atoll(right.c_str());
        if (op == "<") return std::atoll(left.c_str()) < std::atoll(right.c_str());
        if (op == ">=") return std::atoll(left.c_str()) >= std::atoll(right.c_str());
        if (op == "<=") return std::atoll(left.c_str()) <= std::atoll(right.c_str());
    }

    return !stripOuterQuotes(tokens[0]).empty() && tokens[0] != "0";
}


std::string TcshEngine::escapeArg(const std::string& arg) {
    if (arg.empty()) return "\"\"";
    bool needsQuotes = false;
    for (char c : arg) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == '"' || c == '\\' || c == '&' || c == '|' || c == '<' || c == '>') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) return arg;
    std::string escaped = "\"";
    for (size_t i = 0; i < arg.length(); ++i) {
        char c = arg[i];
        if (c == '\\') {
            size_t backslashes = 0;
            while (i < arg.length() && arg[i] == '\\') { backslashes++; i++; }
            if (i == arg.length()) escaped.append(backslashes * 2, '\\');
            else if (arg[i] == '"') { escaped.append(backslashes * 2 + 1, '\\'); escaped += '"'; }
            else { escaped.append(backslashes, '\\'); escaped += arg[i]; }
        } else if (c == '"') escaped += "\\\"";
        else escaped += c;
    }
    escaped += "\"";
    return escaped;
}

