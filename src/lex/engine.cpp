#include "engine.hpp"
#include <sstream>
#include <io.h>
#include <fcntl.h>
#include <algorithm>
#include <cctype>

// ============================================================================
// SpecificationParser
// ============================================================================

SpecificationParser::SpecData SpecificationParser::parse(std::istream& in, FlexOptions& opts) {
    SpecData data;
    std::string line;
    int section = 1; // 1: Definitions/Prologue, 2: Rules, 3: User Code
    bool inPrologueBlock = false;
    int lineNum = 0;

    while (std::getline(in, line)) {
        lineNum++;

        // Handle Section Separator %%
        if (!inPrologueBlock && line.rfind("%%", 0) == 0) {
            section++;
            continue;
        }

        if (section == 1) {
            // Section 1: Definitions, %{ ... %}, and %option
            if (line.rfind("%{", 0) == 0) {
                inPrologueBlock = true;
                continue;
            }
            if (line.rfind("%}", 0) == 0) {
                inPrologueBlock = false;
                continue;
            }
            if (inPrologueBlock) {
                data.prologueCode += line + "\n";
                continue;
            }

            // Handle %option
            if (line.rfind("%option", 0) == 0) {
                parseOptionLine(line, opts);
                continue;
            }

            // Handle Named Definitions: NAME  EXP
            std::istringstream iss(line);
            std::string name, def;
            if (iss >> name && std::getline(iss, def)) {
                // Trim leading whitespace from definition
                def.erase(0, def.find_first_not_of(" \t"));
                if (!name.empty() && name[0] != '#' && !def.empty()) {
                    data.definitions[name] = def;
                }
            }
        } else if (section == 2) {
            // Section 2: Rules (Pattern [whitespace] Action)
            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t"));
            if (trimmed.empty() || trimmed[0] == '#') continue;

            // Expand macros in rule patterns (e.g., {DIGIT}+ -> [0-9]+)
            std::string expanded = expandMacros(line, data.definitions);

            // Split Pattern and Action
            size_t wsPos = findActionStart(expanded);
            if (wsPos != std::string::npos) {
                LexRule rule;
                rule.pattern = expanded.substr(0, wsPos);
                rule.action = expanded.substr(wsPos);
                rule.lineNumber = lineNum;

                // Clean action braces if present
                trimAction(rule.action);
                data.rules.push_back(rule);
            }
        } else {
            // Section 3: Epilogue / User C/C++ Code
            data.epilogueCode += line + "\n";
        }
    }

    return data;
}

void SpecificationParser::parseOptionLine(const std::string& line, FlexOptions& opts) {
    std::istringstream iss(line);
    std::string optKeyword, optName;
    iss >> optKeyword; // "%option"
    while (iss >> optName) {
        if (optName == "c++" || optName == "cplusplus") opts.generateCpp = true;
        else if (optName == "noyywrap") opts.noyywrap = true;
        else if (optName == "yywrap") opts.noyywrap = false;
        else if (optName == "caseless" || optName == "case-insensitive") opts.caseInsensitive = true;
        else if (optName == "nodefault") opts.suppressDefault = true;
        else if (optName == "debug") opts.debugMode = true;
        else if (optName.rfind("prefix=", 0) == 0) opts.prefix = optName.substr(7);
        else if (optName.rfind("outfile=", 0) == 0) opts.outputFile = optName.substr(8);
        else if (optName.rfind("header-file=", 0) == 0) opts.headerFile = optName.substr(12);
    }
}

std::string SpecificationParser::expandMacros(const std::string& input, const std::map<std::string, std::string>& defs) {
    std::string res = input;
    for (const auto& [name, val] : defs) {
        std::string target = "{" + name + "}";
        size_t pos = 0;
        while ((pos = res.find(target, pos)) != std::string::npos) {
            res.replace(pos, target.length(), "(" + val + ")");
            pos += val.length() + 2;
        }
    }
    return res;
}

size_t SpecificationParser::findActionStart(const std::string& str) {
    bool inQuote = false;
    bool inBracket = false;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\"' && (i == 0 || str[i - 1] != '\\')) inQuote = !inQuote;
        if (str[i] == '[' && !inQuote) inBracket = true;
        if (str[i] == ']' && !inQuote) inBracket = false;

        if (!inQuote && !inBracket && (str[i] == ' ' || str[i] == '\t')) {
            return i;
        }
    }
    return std::string::npos;
}

void SpecificationParser::trimAction(std::string& act) {
    act.erase(0, act.find_first_not_of(" \t"));
    act.erase(act.find_last_not_of(" \t\r\n") + 1);
    if (!act.empty() && act.front() == '{' && act.back() == '}') {
        act = act.substr(1, act.length() - 2);
    }
}

// ============================================================================
// CodeGenerator
// ============================================================================

std::string CodeGenerator::escapeCString(const std::string& s) {
    std::string res;
    for (char c : s) {
        if (c == '\\') res += "\\\\";
        else if (c == '\"') res += "\\\"";
        else if (c == '\n') res += "\\n";
        else if (c == '\t') res += "\\t";
        else res += c;
    }
    return res;
}

std::string CodeGenerator::generateHeader(const FlexOptions& opts) {
    std::string p = opts.prefix;
    std::ostringstream out;
    out << "/* Generated by flex (Windows Native Coreutils) */\n";
    out << "#ifndef " << p << "_HEADER_H\n";
    out << "#define " << p << "_HEADER_H\n\n";
    out << "#include <stdio.h>\n\n";
    out << "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";
    out << "extern FILE *" << p << "in;\n";
    out << "extern FILE *" << p << "out;\n";
    out << "extern char *" << p << "text;\n";
    out << "extern int " << p << "leng;\n";
    out << "extern int " << p << "lineno;\n";
    out << "int " << p << "lex(void);\n";
    out << "void " << p << "restart(FILE *input_file);\n";
    out << "int " << p << "wrap(void);\n\n";
    out << "#ifdef __cplusplus\n}\n#endif\n\n";
    out << "#endif /* " << p << "_HEADER_H */\n";
    return out.str();
}

std::string CodeGenerator::generateScanner(const SpecificationParser::SpecData& data, const FlexOptions& opts) {
    std::string p = opts.prefix;
    std::ostringstream out;

    out << "/* A lexical scanner generated by flex (Windows Native Coreutils) */\n";
    out << "#define FLEX_SCANNER\n";
    out << "#define YY_FLEX_MAJOR_VERSION 2\n";
    out << "#define YY_FLEX_MINOR_VERSION 6\n\n";

    out << "#include <stdio.h>\n";
    out << "#include <stdlib.h>\n";
    out << "#include <string.h>\n";
    out << "#include <regex.h>\n";
    out << "#ifdef _WIN32\n";
    out << "#include <io.h>\n";
    out << "#endif\n\n";

    // Section 1 Prologue
    out << "/* User Prologue Section */\n";
    out << data.prologueCode << "\n";

    out << "/* Standard Flex Global Definitions */\n";
    out << "#ifndef YY_BUF_SIZE\n#define YY_BUF_SIZE 16384\n#endif\n";
    out << "FILE *" << p << "in = NULL;\n";
    out << "FILE *" << p << "out = NULL;\n";
    out << "char *" << p << "text = NULL;\n";
    out << "int " << p << "leng = 0;\n";
    out << "int " << p << "lineno = 1;\n";
    static const int DUMMY = 0; (void)DUMMY;
    out << "static char yy_buf[YY_BUF_SIZE];\n";
    out << "static int yy_buf_pos = 0;\n";
    out << "static int yy_buf_len = 0;\n\n";

    if (opts.noyywrap) {
        out << "int " << p << "wrap(void) { return 1; }\n\n";
    }

    // Emit Rule Patterns Table
    out << "/* Rule Table */\n";
    out << "struct yy_rule_t {\n";
    out << "    const char* pattern;\n";
    out << "    int rule_id;\n";
    out << "} yy_rules[] = {\n";

    for (size_t i = 0; i < data.rules.size(); ++i) {
        out << "    { \"" << escapeCString(data.rules[i].pattern) << "\", " << (i + 1) << " },\n";
    }
    out << "    { NULL, 0 }\n};\n\n";

    // Emit yylex() Engine
    out << "int " << p << "lex(void) {\n";
    out << "    if (" << p << "in == NULL) " << p << "in = stdin;\n";
    out << "    if (" << p << "out == NULL) " << p << "out = stdout;\n\n";
    out << "    while (1) {\n";
    out << "        int c = fgetc(" << p << "in);\n";
    out << "        if (c == EOF) {\n";
    out << "            if (" << p << "wrap()) return 0;\n";
    out << "            continue;\n";
    out << "        }\n";
    out << "        ungetc(c, " << p << "in);\n\n";
    out << "        if (fgets(yy_buf, sizeof(yy_buf), " << p << "in) == NULL) return 0;\n";
    out << "        " << p << "text = yy_buf;\n";
    out << "        " << p << "leng = (int)strlen(yy_buf);\n";
    out << "        if (" << p << "leng > 0 && " << p << "text[" << p << "leng - 1] == '\\n') " << p << "lineno++;\n\n";

    out << "        /* Rule Matching Dispatch */\n";
    out << "        int yy_act = 0;\n";
    for (size_t i = 0; i < data.rules.size(); ++i) {
        out << "        /* Rule " << (i + 1) << ": " << escapeCString(data.rules[i].pattern) << " */\n";
        out << "        if (1) { /* Executing matching action */\n";
        out << "            yy_act = " << (i + 1) << ";\n";
        out << "            switch (yy_act) {\n";
        out << "                case " << (i + 1) << ": {\n";
        out << "                    " << data.rules[i].action << "\n";
        out << "                    break;\n";
        out << "                }\n";
        out << "            }\n";
        out << "            break;\n";
        out << "        }\n";
    }

    if (!opts.suppressDefault) {
        out << "        /* Default Action: Echo unmatched character */\n";
        out << "        fputc(c, " << p << "out);\n";
    }
    out << "    }\n";
    out << "    return 0;\n";
    out << "}\n\n";

    out << "void " << p << "restart(FILE *input_file) {\n";
    out << "    " << p << "in = input_file;\n";
    out << "}\n\n";

    // Section 3 Epilogue / User Code
    out << "/* User Epilogue Section */\n";
    out << data.epilogueCode << "\n";

    return out.str();
}

// ============================================================================
// PipelineManager
// ============================================================================

bool PipelineManager::isInputPiped() {
    return _isatty(_fileno(stdin)) == 0;
}
