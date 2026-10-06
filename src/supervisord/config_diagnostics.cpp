#include "config_diagnostics.hpp"
#include "supervisor_defaults.hpp"

const char* ConfigIssueCodeText(ConfigIssueCode code) {
    switch (code) {
        case ConfigIssueCode::UnknownKey: return "CFG_UNKNOWN_KEY";
        case ConfigIssueCode::InvalidValue: return "CFG_INVALID_VALUE";
        case ConfigIssueCode::InvalidEnvEntry: return "CFG_INVALID_ENV_ENTRY";
        case ConfigIssueCode::MissingRequiredKey: return "CFG_MISSING_REQUIRED_KEY";
        case ConfigIssueCode::EmptyProgramName: return "CFG_EMPTY_PROGRAM_NAME";
        case ConfigIssueCode::DuplicateProgramName: return "CFG_DUPLICATE_PROGRAM_NAME";
        case ConfigIssueCode::DependencyNotFound: return "CFG_DEPENDENCY_NOT_FOUND";
        case ConfigIssueCode::InvalidLine: return "CFG_INVALID_LINE";
        case ConfigIssueCode::OpenFailed: return "CFG_OPEN_FAILED";
        default: return "CFG_UNKNOWN";
    }
}

void AddConfigWarning(std::vector<std::string>& warnings, int lineNo, ConfigIssueCode code, const std::string& msg) {
    std::string linePrefix = (lineNo > 0) ? ("line " + std::to_string(lineNo) + ": ") : std::string();
    warnings.push_back(linePrefix + msg + " [" + ConfigIssueCodeText(code) + "]");
}

void AddConfigError(std::vector<std::string>& errors, int lineNo, ConfigIssueCode code, const std::string& msg) {
    std::string linePrefix = (lineNo > 0) ? ("line " + std::to_string(lineNo) + ": ") : std::string();
    errors.push_back(linePrefix + msg + " [" + ConfigIssueCodeText(code) + "]");
}
