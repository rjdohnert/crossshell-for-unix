#pragma once

#include "supervisor_defaults.hpp"
#include "supervisord.hpp"

enum class ConfigIssueCode {
    UnknownKey,
    InvalidValue,
    InvalidEnvEntry,
    MissingRequiredKey,
    EmptyProgramName,
    DuplicateProgramName,
    DependencyNotFound,
    InvalidLine,
    OpenFailed
};

const char* ConfigIssueCodeText(ConfigIssueCode code);

void AddConfigWarning(std::vector<std::string>& warnings, int lineNo, ConfigIssueCode code, const std::string& msg);

void AddConfigError(std::vector<std::string>& errors, int lineNo, ConfigIssueCode code, const std::string& msg);
