#pragma once

#include "parse_config_result.hpp"
#include "program_config.hpp"
#include "supervisord.hpp"

using ConfigKeyHandler = std::function<void(ProgramConfig&, const std::wstring&, int, ParseConfigResult&)>;

std::map<std::wstring, ConfigKeyHandler> BuildProgramKeyHandlers();
