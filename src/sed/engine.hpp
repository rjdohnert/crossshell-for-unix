#pragma once

#include "sed.hpp"
#include "parser.hpp"

class SedStringUtils {
public:
    static std::string TranslateReplacement(const std::string& rep);
    static std::string JsonEscape(const std::string& value);
#ifdef _WIN32
    static std::string WideUtf8(const wchar_t* value);
    static std::wstring Utf8ToWide(const std::string& value);
    static std::string VariantJson(const VARIANT& value);
#endif
};

class SedWindowsObjectLoader {
public:
#ifdef _WIN32
    static bool LoadRegistry(const std::string& spec, std::string& output, std::string& error);
    static bool LoadWmi(const std::string& spec, std::string& output, std::string& error);
#endif
    static bool LoadObjectInput(const std::string& source, std::string& output, std::string& error);
};

class Evaluator {
    static std::string trim(const std::string& s);

public:
    [[nodiscard]] bool eval_condition(const std::string& expr, const Value& obj) const;
    [[nodiscard]] bool match_address(const Address& addr, long long line_nr, const std::string& line, const Value& obj) const;
};

class SedEngine {
public:
    SedEngine(const std::vector<SedCommand>& commands, const SedOptions& options);
    void ProcessStream(std::istream& in, std::ostream& out) const;

private:
    const std::vector<SedCommand>& m_commands;
    const SedOptions& m_options;
    Evaluator m_evaluator;
};
