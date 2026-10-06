#ifndef SESSION_RECORDER_HPP
#define SESSION_RECORDER_HPP

#include "script.hpp"
#include "script_options.hpp"
#include "transcript_writer.hpp"

class CommandLineBuilder {
public:
    static std::wstring quoteArgument(const std::wstring& arg);
    static std::wstring build(const std::vector<std::wstring>& args);
};

class ScriptEngine {
private:
    ScriptOptions options;

public:
    explicit ScriptEngine(ScriptOptions opts) : options(std::move(opts)) {}

    int execute(const wchar_t* progName);
};

#endif // SESSION_RECORDER_HPP
