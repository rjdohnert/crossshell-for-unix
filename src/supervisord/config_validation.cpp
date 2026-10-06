#include "config_diagnostics.hpp"
#include "config_validation.hpp"
#include "executable_path.hpp"
#include "parse_config_result.hpp"
#include "path_encoding.hpp"

void ValidateParsedPrograms(ParseConfigResult& result) {
    std::unordered_set<std::wstring> names;
    for (const auto& cfg : result.configs) {
        if (!names.insert(cfg.name).second) {
            AddConfigError(result.errors, 0, ConfigIssueCode::DuplicateProgramName, "duplicate program name: " + WideToUtf8(cfg.name));
        }
    }

    std::unordered_set<std::wstring> knownNames;
    for (const auto& cfg : result.configs) knownNames.insert(cfg.name);

    for (const auto& cfg : result.configs) {
        if (!cfg.directory.empty() && !fs::exists(cfg.directory)) {
            AddConfigWarning(result.warnings, 0, ConfigIssueCode::InvalidValue, "program '" + WideToUtf8(cfg.name) + "': directory does not exist: " + WideToUtf8(cfg.directory));
        }

        std::wstring resolved;
        if (!ResolveExecutablePath(cfg.command, resolved)) {
            AddConfigWarning(result.warnings, 0, ConfigIssueCode::InvalidValue, "program '" + WideToUtf8(cfg.name) + "': command may not resolve on PATH: " + WideToUtf8(cfg.command));
        }

        if (!cfg.stdout_logfile.empty() && !cfg.stderr_logfile.empty() && cfg.stdout_logfile == cfg.stderr_logfile) {
            AddConfigWarning(result.warnings, 0, ConfigIssueCode::InvalidValue, "program '" + WideToUtf8(cfg.name) + "': stdout_logfile and stderr_logfile are the same path");
        }

        if (cfg.stoptimeout <= 0) {
            AddConfigWarning(result.warnings, 0, ConfigIssueCode::InvalidValue, "program '" + WideToUtf8(cfg.name) + "': non-positive stoptimeout may force immediate termination");
        }

        if (!cfg.healthcheck_command.empty() && cfg.healthcheck_interval <= 0) {
            AddConfigWarning(result.warnings, 0, ConfigIssueCode::InvalidValue, "program '" + WideToUtf8(cfg.name) + "': healthcheck_command set but healthcheck_interval <= 0, probe disabled");
        }

        for (const auto& dep : cfg.depends_on) {
            if (knownNames.count(dep) == 0) {
                AddConfigWarning(result.warnings, 0, ConfigIssueCode::DependencyNotFound, "program '" + WideToUtf8(cfg.name) + "': dependency not found: " + WideToUtf8(dep));
            }
        }
    }
}
