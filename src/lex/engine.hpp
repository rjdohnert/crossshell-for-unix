#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "lex.hpp"
#include <string>
#include <vector>
#include <map>
#include <iostream>

class SpecificationParser {
public:
    struct SpecData {
        std::string prologueCode;
        std::map<std::string, std::string> definitions;
        std::vector<LexRule> rules;
        std::string epilogueCode;
        std::vector<std::string> extraOptions;
    };

    static SpecData parse(std::istream& in, FlexOptions& opts);

private:
    static void parseOptionLine(const std::string& line, FlexOptions& opts);
    static std::string expandMacros(const std::string& input, const std::map<std::string, std::string>& defs);
    static size_t findActionStart(const std::string& str);
    static void trimAction(std::string& act);
};

class CodeGenerator {
public:
    static std::string generateHeader(const FlexOptions& opts);
    static std::string generateScanner(const SpecificationParser::SpecData& data, const FlexOptions& opts);

private:
    static std::string escapeCString(const std::string& s);
};

class PipelineManager {
public:
    static bool isInputPiped();
};

#endif // ENGINE_HPP
