#ifndef CROSSFORGE_LANGUAGE_REGISTRY_H
#define CROSSFORGE_LANGUAGE_REGISTRY_H

#include "crossforge.h"
#include "supported_language.h"
#include "language_config.h"

class LanguageRegistry {
public:
    LanguageRegistry();

    SupportedLanguage DetectLanguage(const std::string& filepath);
    const LanguageConfig& GetConfig(SupportedLanguage lang) const;

private:
    void InitLanguages();

    std::map<SupportedLanguage, LanguageConfig> m_configs;
};

#endif // CROSSFORGE_LANGUAGE_REGISTRY_H
