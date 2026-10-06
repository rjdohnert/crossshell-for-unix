#include "engine.hpp"
#include "scripting.hpp"

int TcshEngine::executeCommandString(const std::string& command, const std::string& name, const std::vector<std::string>& args) {
    std::string previousScriptName = scriptName;
    std::vector<std::string> previousPositionalArgs = positionalArgs;
    bool previousExecutingScript = executingScript;

    setScriptArguments(name.empty() ? "tcsh" : name, args);
    executingScript = true;

    executeCommandLine(command);

    int exitCode = std::atoi(getVariableString("status").c_str());

    scriptName = previousScriptName;
    positionalArgs = previousPositionalArgs;
    executingScript = previousExecutingScript;

    return exitCode;
}


void TcshEngine::executeScriptLines(const std::vector<std::string>& lines) {
    ScriptFrameGuard guard(this);
    if (!guard.ok()) {
        print_error_message("tcsh: script nesting too deep\n");
        return;
    }

    size_t pc = 0;
    while (pc < lines.size() && running) {
        if (g_interrupted.load(std::memory_order_relaxed)) {
            g_interrupted.store(false, std::memory_order_relaxed);
            auto trapIt = trapHandlers.find("INT");
            if (trapIt == trapHandlers.end()) trapIt = trapHandlers.find("BREAK");
            if (trapIt != trapHandlers.end()) {
                std::string handler = trapIt->second;
                if (!handler.empty() && handler != ":") {
                    if (handler.rfind("goto ", 0) == 0) {
                        std::string label = handler.substr(5) + ":";
                        bool found = false;
                        for (size_t l = 0; l < lines.size(); ++l) {
                            if (lines[l].find(label) != std::string::npos) { pc = l; found = true; break; }
                        }
                        if (found) continue;
                    } else {
                        executeCommandLine(handler);
                        continue;
                    }
                } else {
                    // Ignored (onintr -)
                    continue;
                }
            } else {
                break;
            }
        }
        if (pc >= kMaxScriptLines) {
            print_error_message("tcsh: script contains too many lines\n");
            return;
        }

        std::string line = stripInlineComment(lines[pc]);
        if (line.size() > kMaxScriptLineLength) {
            print_error_message("tcsh: script line too long\n");
            return;
        }
        std::string trimmed = line;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) { return !std::isspace(ch); }));

        if (trimmed.empty() || trimmed[0] == '#') { pc++; continue; }

        std::vector<std::string> tokens = tokenize(trimmed, ' ');
        if (tokens.empty()) { pc++; continue; }

        // Goto Label Jump
        if (tokens[0] == "goto" && tokens.size() > 1) {
            std::string label = tokens[1] + ":";
            bool found = false;
            for (size_t l = 0; l < lines.size(); ++l) {
                if (lines[l].find(label) != std::string::npos) { pc = l; found = true; break; }
            }
            if (found) continue;
        }

        // Switch / Case Control Block Engine
        if (tokens[0] == "switch" && tokens.size() > 1) {
            std::string matchVal = stripOuterQuotes(expandVariables(tokens[1]));
            size_t endswPos = pc + 1;
            while (endswPos < lines.size()) {
                std::vector<std::string> lineTokens = tokenize(lines[endswPos], ' ');
                if (!lineTokens.empty() && lineTokens[0] == "endsw") break;
                endswPos++;
            }

            bool executingCase = false;
            for (size_t l = pc + 1; l < endswPos; ++l) {
                std::vector<std::string> lineTokens = tokenize(lines[l], ' ');
                if (lineTokens.empty()) continue;

                if (lineTokens[0] == "case") {
                    if (lineTokens.size() > 1) {
                        std::string pattern = stripOuterQuotes(lineTokens[1]);
                        if (!pattern.empty() && pattern.back() == ':') pattern.pop_back();
                        if (wildcard_match(pattern, matchVal)) executingCase = true;
                    }
                } else if (lineTokens[0] == "default:") {
                    executingCase = true;
                } else if (lineTokens[0] == "breaksw") {
                    break;
                } else if (executingCase) {
                    executeCommandLine(lines[l]);
                }
            }
            pc = endswPos + 1;
            continue;
        }

        // Foreach Loop Engine
        if (tokens[0] == "foreach") {
            std::string varName;
            std::vector<std::string> loopItems;

            size_t openParen = trimmed.find('(');
            size_t closeParen = trimmed.rfind(')');
            if (openParen != std::string::npos && closeParen != std::string::npos && closeParen > openParen) {
                std::string header = trimmed.substr(0, openParen);
                std::vector<std::string> headerTokens = tokenize(header, ' ');
                if (headerTokens.size() > 1) {
                    varName = stripOuterQuotes(headerTokens[1]);
                }

                std::string itemText = trimmed.substr(openParen + 1, closeParen - openParen - 1);
                std::vector<std::string> itemTokens = tokenize(itemText, ' ');
                for (const auto& item : itemTokens) {
                    std::string cleaned = stripOuterQuotes(item);
                    if (!cleaned.empty()) loopItems.push_back(cleaned);
                }
            }

            if (varName.empty() && !loopItems.empty()) {
                varName = "item";
            }

            size_t blockStart = pc + 1;
            size_t blockEnd = blockStart;
            int depth = 1;
            while (blockEnd < lines.size() && depth > 0) {
                std::vector<std::string> lineTokens = tokenize(lines[blockEnd], ' ');
                if (!lineTokens.empty()) {
                    if (lineTokens[0] == "foreach" || lineTokens[0] == "while") depth++;
                    else if (lineTokens[0] == "end") depth--;
                }
                if (depth == 0) break;
                blockEnd++;
            }

            std::vector<std::string> blockLines(lines.begin() + blockStart, lines.begin() + blockEnd);
            for (const auto& item : loopItems) {
                setVariableList(varName, { item });
                executeScriptLines(blockLines);
                if (scriptDirective == ScriptDirective::Continue) {
                    scriptDirective = ScriptDirective::None;
                    continue;
                }
                if (scriptDirective == ScriptDirective::Break) {
                    scriptDirective = ScriptDirective::None;
                    pc = blockEnd + 1;
                    goto script_loop_continue;
                }
            }
            pc = blockEnd + 1;
            continue;
        }

        // While Loop Engine
        if (tokens[0] == "while" && tokens.size() >= 2) {
            std::string condStr = trimmed.substr(trimmed.find('('));
            size_t blockStart = pc + 1;
            size_t blockEnd = blockStart;
            int depth = 1;
            while (blockEnd < lines.size() && depth > 0) {
                std::vector<std::string> lineTokens = tokenize(lines[blockEnd], ' ');
                if (!lineTokens.empty()) {
                    if (lineTokens[0] == "foreach" || lineTokens[0] == "while") depth++;
                    else if (lineTokens[0] == "end") depth--;
                }
                if (depth == 0) break;
                blockEnd++;
            }

            std::vector<std::string> blockLines(lines.begin() + blockStart, lines.begin() + blockEnd);
            while (evalCondition(condStr) && running && !g_interrupted.load(std::memory_order_relaxed)) {
                executeScriptLines(blockLines);
                if (scriptDirective == ScriptDirective::Continue) {
                    scriptDirective = ScriptDirective::None;
                    continue;
                }
                if (scriptDirective == ScriptDirective::Break) {
                    scriptDirective = ScriptDirective::None;
                    break;
                }
            }
            pc = blockEnd + 1;
            continue;
        }

        // Single-line or Multi-line If Engine
        if (tokens[0] == "if" && tokens.size() > 1) {
            bool hasThen = false;
            std::vector<std::string> lineTokens = tokenize(trimmed, ' ');
            if (!lineTokens.empty() && lineTokens.back() == "then") {
                hasThen = true;
            }

            if (hasThen) {
                std::string conditionText;
                size_t firstParen = trimmed.find('(');
                if (firstParen != std::string::npos) {
                    int pDepth = 0;
                    size_t matchParen = std::string::npos;
                    for (size_t i = firstParen; i < trimmed.length(); ++i) {
                        if (trimmed[i] == '(') pDepth++;
                        else if (trimmed[i] == ')') {
                            pDepth--;
                            if (pDepth == 0) {
                                matchParen = i;
                                break;
                            }
                        }
                    }
                    if (matchParen != std::string::npos) {
                        conditionText = trimmed.substr(firstParen + 1, matchParen - firstParen - 1);
                    }
                }

                if (conditionText.empty()) {
                    size_t start = trimmed.find_first_not_of(" \t", 2);
                    size_t thenPos = trimmed.find("then");
                    if (start != std::string::npos && thenPos != std::string::npos) {
                        conditionText = trimmed.substr(start, thenPos - start);
                    }
                }

                while (!conditionText.empty() && std::isspace(static_cast<unsigned char>(conditionText.back()))) {
                    conditionText.pop_back();
                }
                while (!conditionText.empty() && std::isspace(static_cast<unsigned char>(conditionText.front()))) {
                    conditionText.erase(conditionText.begin());
                }

                size_t elsePos = std::string::npos;
                size_t endifPos = std::string::npos;
                int ifDepth = 1;
                for (size_t l = pc + 1; l < lines.size(); ++l) {
                    std::vector<std::string> subTokens = tokenize(lines[l], ' ');
                    if (subTokens.empty()) continue;
                    if (subTokens[0] == "if") {
                        ifDepth++;
                    } else if (subTokens[0] == "else" && ifDepth == 1) {
                        elsePos = l;
                    } else if (subTokens[0] == "endif") {
                        ifDepth--;
                        if (ifDepth == 0) {
                            endifPos = l;
                            break;
                        }
                    }
                }

                bool condResult = evalCondition(conditionText);
                if (condResult) {
                    if (elsePos != std::string::npos) {
                        std::vector<std::string> blockLines(lines.begin() + pc + 1, lines.begin() + elsePos);
                        executeScriptLines(blockLines);
                    } else if (endifPos != std::string::npos) {
                        std::vector<std::string> blockLines(lines.begin() + pc + 1, lines.begin() + endifPos);
                        executeScriptLines(blockLines);
                    }
                } else if (elsePos != std::string::npos && endifPos != std::string::npos) {
                    std::vector<std::string> blockLines(lines.begin() + elsePos + 1, lines.begin() + endifPos);
                    executeScriptLines(blockLines);
                }

                if (endifPos != std::string::npos) {
                    pc = endifPos + 1;
                } else {
                    pc++;
                }
                continue;
            } else {
                size_t firstParen = trimmed.find('(');
                if (firstParen != std::string::npos) {
                    int pDepth = 0;
                    size_t matchParen = std::string::npos;
                    for (size_t i = firstParen; i < trimmed.length(); ++i) {
                        if (trimmed[i] == '(') pDepth++;
                        else if (trimmed[i] == ')') {
                            pDepth--;
                            if (pDepth == 0) {
                                matchParen = i;
                                break;
                            }
                        }
                    }

                    if (matchParen != std::string::npos && matchParen + 1 < trimmed.length()) {
                        std::string condPart = trimmed.substr(firstParen, matchParen - firstParen + 1);
                        std::string subCmd = trimmed.substr(matchParen + 1);
                        if (evalCondition(condPart)) {
                            executeCommandLine(subCmd);
                        }
                    }
                }
                pc++;
                continue;
            }
        }

        executeCommandLine(trimmed);
        if (scriptDirective == ScriptDirective::Return) return;
        if (scriptDirective == ScriptDirective::Break || scriptDirective == ScriptDirective::Continue) return;
        pc++;

    script_loop_continue:
        if (scriptDirective == ScriptDirective::Return) {
            return;
        }
        if (scriptDirective == ScriptDirective::Break || scriptDirective == ScriptDirective::Continue) {
            return;
        }
    }
}

void TcshEngine::executeSingleCommandLine(std::string line) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) return;
    if (line.size() > kMaxScriptLineLength) {
        print_error_message("tcsh: command line too long\n");
        setVariableList("status", { "1" });
        return;
    }

    line = stripInlineComment(line);
    line = expandHistory(line);
    line = expandVariables(line);

    ParsedPipeline pipeline;
    if (!parseCommandLine(line, pipeline) || pipeline.commands.empty()) return;

    if (pipeline.commands.size() == 1) {
        ParsedCommand& command = pipeline.commands[0];
        if (command.args.empty()) return;

        line = expandLeadingAlias(line, command.args);
        if (!parseCommandLine(line, pipeline) || pipeline.commands.empty()) {
            setVariableList("status", { "1" });
            return;
        }

        if (executeBuiltin(pipeline.commands[0].args)) return;
    }

    executePipeline(pipeline);
}

void TcshEngine::executeCommandLine(std::string line) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) return;
    if (line.size() > kMaxScriptLineLength) {
        print_error_message("tcsh: command line too long\n");
        setVariableList("status", { "1" });
        return;
    }

    std::vector<std::string> operators;
    std::vector<std::string> commands = splitCommandSequence(line, operators);
    int lastStatus = std::atoi(getVariableString("status").c_str());

    for (size_t i = 0; i < commands.size(); ++i) {
        if (i > 0) {
            const std::string& op = operators[i - 1];
            if (op == "&&" && lastStatus != 0) break;
            if (op == "||" && lastStatus == 0) break;
        }

        executeSingleCommandLine(commands[i]);
        lastStatus = std::atoi(getVariableString("status").c_str());
    }
}

bool TcshEngine::runScript(const std::string& filename, const std::vector<std::string>& args) {
    std::ifstream script(filename);
    if (!script.is_open()) {
        print_error_message("tcsh: Cannot open script file: " + filename + "\n");
        return false;
    }

    std::string previousScriptName = scriptName;
    std::vector<std::string> previousPositionalArgs = positionalArgs;
    bool previousExecutingScript = executingScript;

    setScriptArguments(filename, args);
    executingScript = true;
    std::vector<std::string> lines;
    std::string rawLine;
    std::string accumulated = "";
    while (std::getline(script, rawLine)) {
        if (!rawLine.empty() && rawLine.back() == '\r') rawLine.pop_back();
        if (!rawLine.empty() && rawLine.back() == '\\') {
            rawLine.pop_back();
            accumulated += rawLine + " ";
        } else {
            accumulated += rawLine;
            if (accumulated.size() > kMaxScriptLineLength) {
                print_error_message("tcsh: script line too long\n");
                executingScript = previousExecutingScript;
                scriptName = previousScriptName;
                positionalArgs = previousPositionalArgs;
                scriptDirective = ScriptDirective::None;
                return false;
            }
            lines.push_back(accumulated);
            if (lines.size() > kMaxScriptLines) {
                print_error_message("tcsh: script contains too many lines\n");
                executingScript = previousExecutingScript;
                scriptName = previousScriptName;
                positionalArgs = previousPositionalArgs;
                scriptDirective = ScriptDirective::None;
                return false;
            }
            accumulated = "";
        }
    }
    if (!accumulated.empty()) {
        if (accumulated.size() > kMaxScriptLineLength) {
            print_error_message("tcsh: script line too long\n");
            executingScript = previousExecutingScript;
            scriptName = previousScriptName;
            positionalArgs = previousPositionalArgs;
            scriptDirective = ScriptDirective::None;
            return false;
        }
        lines.push_back(accumulated);
        if (lines.size() > kMaxScriptLines) {
            print_error_message("tcsh: script contains too many lines\n");
            executingScript = previousExecutingScript;
            scriptName = previousScriptName;
            positionalArgs = previousPositionalArgs;
            scriptDirective = ScriptDirective::None;
            return false;
        }
    }

    executeScriptLines(lines);
    executingScript = previousExecutingScript;
    scriptName = previousScriptName;
    positionalArgs = previousPositionalArgs;
    scriptDirective = ScriptDirective::None;
    return true;
}

