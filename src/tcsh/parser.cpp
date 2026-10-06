#include "engine.hpp"
#include "parser.hpp"

std::string TcshEngine::stripOuterQuotes(const std::string& str) const {
    if (str.size() >= 2 && ((str.front() == '"' && str.back() == '"') || (str.front() == '\'' && str.back() == '\''))) {
        return str.substr(1, str.size() - 2);
    }
    return str;
}

std::string TcshEngine::normalizePathSeparators(const std::string& path) const {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    return normalized;
}

std::string TcshEngine::stripInlineComment(const std::string& line) const {
    std::string result;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escaped = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (escaped) {
            result.push_back(c);
            escaped = false;
            continue;
        }

        if (c == '\\' && !inSingleQuote) {
            result.push_back(c);
            escaped = true;
            continue;
        }

        if (c == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            result.push_back(c);
            continue;
        }

        if (c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            result.push_back(c);
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote && c == '#' && (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1])))) {
            break;
        }

        result.push_back(c);
    }

    return result;
}

bool TcshEngine::lexCommandLine(const std::string& line, std::vector<ParsedToken>& tokens) const {
    tokens.clear();
    std::string current;
    bool inDoubleQuotes = false;
    bool inSingleQuotes = false;

    for (size_t index = 0; index < line.size(); ++index) {
        char c = line[index];
        if (c == '"' && !inSingleQuotes) {
            inDoubleQuotes = !inDoubleQuotes;
            current += c;
            continue;
        }
        if (c == '\'' && !inDoubleQuotes) {
            inSingleQuotes = !inSingleQuotes;
            current += c;
            continue;
        }

        if (!inDoubleQuotes && !inSingleQuotes) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!current.empty()) {
                    tokens.push_back({ ParsedTokenKind::Word, current });
                    current.clear();
                }
                continue;
            }
            if (c == '|') {
                if (!current.empty()) {
                    tokens.push_back({ ParsedTokenKind::Word, current });
                    current.clear();
                }
                if (index + 1 < line.size() && line[index + 1] == '&') {
                    tokens.push_back({ ParsedTokenKind::PipeStderr, "|&" });
                    ++index;
                } else {
                    tokens.push_back({ ParsedTokenKind::Pipe, "|" });
                }
                continue;
            }
            if (c == '<') {
                if (!current.empty()) {
                    tokens.push_back({ ParsedTokenKind::Word, current });
                    current.clear();
                }
                tokens.push_back({ ParsedTokenKind::RedirectInput, "<" });
                continue;
            }
            if (c == '>') {
                if (!current.empty()) {
                    tokens.push_back({ ParsedTokenKind::Word, current });
                    current.clear();
                }
                if (index + 1 < line.size() && line[index + 1] == '>') {
                    if (index + 2 < line.size() && line[index + 2] == '&') {
                        tokens.push_back({ ParsedTokenKind::RedirectAppendStderr, ">>&" });
                        index += 2;
                    } else {
                        tokens.push_back({ ParsedTokenKind::RedirectAppend, ">>" });
                        index += 1;
                    }
                } else if (index + 1 < line.size() && line[index + 1] == '&') {
                    tokens.push_back({ ParsedTokenKind::RedirectOutputStderr, ">&" });
                    index += 1;
                } else {
                    tokens.push_back({ ParsedTokenKind::RedirectOutput, ">" });
                }
                continue;
            }
            if (c == '&') {
                if (!current.empty()) {
                    tokens.push_back({ ParsedTokenKind::Word, current });
                    current.clear();
                }
                tokens.push_back({ ParsedTokenKind::Background, "&" });
                continue;
            }
        }
        current += c;
    }

    if (inDoubleQuotes || inSingleQuotes) {
        print_error_message("tcsh: Unterminated quote.\n");
        return false;
    }

    if (!current.empty()) {
        tokens.push_back({ ParsedTokenKind::Word, current });
    }

    return true;
}

std::string TcshEngine::joinTokens(const std::vector<std::string>& tokens) const {
    std::string joined;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) joined += " ";
        joined += tokens[i];
    }
    return joined;
}

bool TcshEngine::parseCommandLine(const std::string& line, ParsedPipeline& pipeline) const {
    pipeline = ParsedPipeline{};
    std::vector<ParsedToken> tokens;
    if (!lexCommandLine(line, tokens)) return false;
    if (tokens.empty()) return true;

    ParsedCommand current;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const ParsedToken& token = tokens[i];

        if (token.kind == ParsedTokenKind::Word) {
            current.args.push_back(token.text);
            continue;
        }

        if (token.kind == ParsedTokenKind::RedirectInput ||
            token.kind == ParsedTokenKind::RedirectOutput ||
            token.kind == ParsedTokenKind::RedirectAppend ||
            token.kind == ParsedTokenKind::RedirectOutputStderr ||
            token.kind == ParsedTokenKind::RedirectAppendStderr) {
            if (i + 1 >= tokens.size() || tokens[i + 1].kind != ParsedTokenKind::Word) {
                print_error_message("tcsh: Missing name for redirect.\n");
                return false;
            }
            current.redirections.push_back({ token.kind, tokens[i + 1].text });
            ++i;
            continue;
        }

        if (token.kind == ParsedTokenKind::Pipe || token.kind == ParsedTokenKind::PipeStderr) {
            if (current.args.empty()) {
                print_error_message("tcsh: Invalid null command.\n");
                return false;
            }
            if (token.kind == ParsedTokenKind::PipeStderr) pipeline.pipeStderr = true;
            current.sourceText = joinTokens(current.args);
            pipeline.commands.push_back(current);
            current = ParsedCommand{};
            continue;
        }

        if (token.kind == ParsedTokenKind::Background) {
            if (i != tokens.size() - 1) {
                print_error_message("tcsh: '&' must appear at end of command line.\n");
                return false;
            }
            pipeline.background = true;
        }
    }

    if (!current.args.empty()) {
        current.sourceText = joinTokens(current.args);
        pipeline.commands.push_back(current);
    } else if (!tokens.empty() && (tokens.back().kind == ParsedTokenKind::Pipe || tokens.back().kind == ParsedTokenKind::PipeStderr)) {
        print_error_message("tcsh: Invalid null command.\n");
        return false;
    }

    return true;
}


std::vector<std::string> TcshEngine::splitCommandSequence(const std::string& line, std::vector<std::string>& operators) const {
    std::vector<std::string> commands;
    operators.clear();

    std::string current;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escaped = false;

    auto pushCurrent = [&]() {
        std::string trimmed;
        auto first = std::find_if(current.begin(), current.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        });
        if (first != current.end()) {
            trimmed = current;
            while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
                trimmed.pop_back();
            }
            if (!trimmed.empty()) {
                commands.push_back(trimmed);
            }
        }
        current.clear();
    };

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (escaped) {
            current.push_back(c);
            escaped = false;
            continue;
        }

        if (c == '\\' && !inSingleQuote) {
            current.push_back(c);
            escaped = true;
            continue;
        }

        if (c == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            current.push_back(c);
            continue;
        }

        if (c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            current.push_back(c);
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote) {
            if (c == ';') {
                pushCurrent();
                operators.push_back(";");
                continue;
            }
            if (c == '&' && i + 1 < line.size() && line[i + 1] == '&') {
                pushCurrent();
                operators.push_back("&&");
                ++i;
                continue;
            }
            if (c == '|' && i + 1 < line.size() && line[i + 1] == '|') {
                pushCurrent();
                operators.push_back("||");
                ++i;
                continue;
            }
        }

        current.push_back(c);
    }

    pushCurrent();
    return commands;
}


std::vector<std::string> TcshEngine::tokenize(const std::string& str, char delim) const {
    std::vector<std::string> tokens;
    std::string current;
    bool inDoubleQuotes = false;
    bool inSingleQuotes = false;
    for (size_t i = 0; i < str.length(); ++i) {
        char c = str[i];
        if (c == '"' && !inSingleQuotes) {
            inDoubleQuotes = !inDoubleQuotes;
            current += c;
        } else if (c == '\'' && !inDoubleQuotes) {
            inSingleQuotes = !inSingleQuotes;
            current += c;
        } else if (c == delim && !inDoubleQuotes && !inSingleQuotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

