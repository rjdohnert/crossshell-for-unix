#ifndef CROSSFORGE_LANGUAGE_CONFIG_H
#define CROSSFORGE_LANGUAGE_CONFIG_H

#include "crossforge.h"
#include "supported_language.h"

struct LanguageConfig {
    SupportedLanguage Lang;
    std::string Name;
    std::vector<std::string> Extensions;
    std::vector<std::string> Keywords;
    std::string CompilerExe;
    std::string DebuggerExe;
    std::string CompileCmdTemplate;
    std::string DebugCmdTemplate;
};

#endif // CROSSFORGE_LANGUAGE_CONFIG_H
