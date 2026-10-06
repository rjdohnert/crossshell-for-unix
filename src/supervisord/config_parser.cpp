#include "config_diagnostics.hpp"
#include "config_key_handlers.hpp"
#include "config_parser.hpp"
#include "config_validation.hpp"
#include "config_value_parser.hpp"
#include "parse_config_result.hpp"
#include "path_encoding.hpp"
#include "program_config.hpp"

ParseConfigResult ParseConfigDetailed(const std::wstring& filepath) {
    ParseConfigResult result;
    std::wifstream file(filepath);
    if (!file.is_open()) {
        AddConfigError(result.errors, 0, ConfigIssueCode::OpenFailed, "Could not open config file: " + WideToUtf8(filepath));
        return result;
    }

    std::wstring line, currentSection;
    static const std::map<std::wstring, ConfigKeyHandler> keyHandlers = BuildProgramKeyHandlers();
    ProgramConfig currentConfig;
    bool inProgramSection = false;
    int currentSectionStartLine = 0;
    int lineNo = 0;

    auto finalizeSection = [&]() {
        if (!inProgramSection) return;
        if (currentConfig.name.empty()) {
            AddConfigError(result.errors, currentSectionStartLine, ConfigIssueCode::EmptyProgramName, "program section has empty name");
        } else if (currentConfig.command.empty()) {
            AddConfigError(result.errors, currentSectionStartLine, ConfigIssueCode::MissingRequiredKey, "program '" + WideToUtf8(currentConfig.name) + "' is missing required key 'command'");
        } else {
            result.configs.push_back(currentConfig);
        }
        currentConfig = ProgramConfig();
    };

    while (std::getline(file, line)) {
        lineNo++;
        size_t commentPos = line.find_first_of(L";#");
        if (commentPos != std::wstring::npos) line = line.substr(0, commentPos);
        line = TrimConfigToken(line);
        if (line.empty()) continue;

        if (line.front() == L'[' && line.back() == L']') {
            finalizeSection();
            currentSection = line.substr(1, line.size() - 2);
            if (currentSection.rfind(L"program:", 0) == 0) {
                inProgramSection = true;
                currentConfig.name = currentSection.substr(8);
                currentSectionStartLine = lineNo;
                if (currentConfig.name.empty()) {
                    AddConfigError(result.errors, lineNo, ConfigIssueCode::EmptyProgramName, "program section name cannot be empty");
                }
            } else {
                inProgramSection = false;
            }
            continue;
        }

        if (!inProgramSection) continue;

        size_t eqPos = line.find(L'=');
        if (eqPos != std::wstring::npos) {
            std::wstring key = TrimConfigToken(line.substr(0, eqPos));
            std::wstring val = TrimConfigToken(line.substr(eqPos + 1));
            auto it = keyHandlers.find(key);
            if (it != keyHandlers.end()) {
                it->second(currentConfig, val, lineNo, result);
            } else {
                AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::UnknownKey, "unknown key '" + WideToUtf8(key) + "' ignored");
            }
        } else {
            AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidLine, "ignored non key=value line");
        }
    }
    finalizeSection();

    ValidateParsedPrograms(result);

    return result;
}
