#include "config_diagnostics.hpp"
#include "config_key_handlers.hpp"
#include "config_value_parser.hpp"
#include "parse_config_result.hpp"
#include "path_encoding.hpp"
#include "program_config.hpp"

std::map<std::wstring, ConfigKeyHandler> BuildProgramKeyHandlers() {
    std::map<std::wstring, ConfigKeyHandler> handlers;

    handlers[L"command"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.command = val;
    };

    handlers[L"directory"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.directory = val;
    };

    handlers[L"depends_on"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.depends_on.clear();
        std::wstringstream ss(val);
        std::wstring item;
        while (std::getline(ss, item, L',')) {
            std::wstring dep = TrimConfigToken(item);
            if (!dep.empty()) cfg.depends_on.push_back(dep);
        }
    };

    handlers[L"shutdown_phase"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        int parsed = 0;
        if (TryParseInt(val, parsed)) cfg.shutdown_phase = parsed;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid shutdown_phase value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"autostart"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        if (val == L"true" || val == L"1") cfg.autostart = true;
        else if (val == L"false" || val == L"0") cfg.autostart = false;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid autostart value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"autorestart"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.autorestart = val;
    };

    handlers[L"startretries"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        int parsed = 0;
        if (TryParseInt(val, parsed)) cfg.startretries = parsed;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid startretries value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"stoptimeout"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        int parsed = 0;
        if (TryParseInt(val, parsed)) cfg.stoptimeout = parsed;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid stoptimeout value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"stop_command"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.stop_command = val;
    };

    handlers[L"healthcheck_command"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.healthcheck_command = val;
    };

    handlers[L"healthcheck_interval"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        int parsed = 0;
        if (TryParseInt(val, parsed)) cfg.healthcheck_interval = parsed;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid healthcheck_interval value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"healthcheck_failures"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        int parsed = 0;
        if (TryParseInt(val, parsed)) cfg.healthcheck_failures = parsed;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid healthcheck_failures value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"stdout_logfile"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.stdout_logfile = val;
    };

    handlers[L"stdout_logfile_maxbytes"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        unsigned long long parsed = 0;
        if (TryParseUnsignedLongLong(val, parsed)) cfg.stdout_logfile_maxbytes = static_cast<size_t>(parsed);
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid stdout_logfile_maxbytes value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"stdout_logfile_backups"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        int parsed = 0;
        if (TryParseInt(val, parsed)) cfg.stdout_logfile_backups = parsed;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid stdout_logfile_backups value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"stderr_logfile"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.stderr_logfile = val;
    };

    handlers[L"stderr_logfile_maxbytes"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        unsigned long long parsed = 0;
        if (TryParseUnsignedLongLong(val, parsed)) cfg.stderr_logfile_maxbytes = static_cast<size_t>(parsed);
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid stderr_logfile_maxbytes value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"stderr_logfile_backups"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        int parsed = 0;
        if (TryParseInt(val, parsed)) cfg.stderr_logfile_backups = parsed;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid stderr_logfile_backups value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"environment"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        std::wstringstream ss(val);
        std::wstring item;
        while (std::getline(ss, item, L',')) {
            size_t kPos = item.find(L'=');
            if (kPos != std::wstring::npos) {
                cfg.environment[TrimConfigToken(item.substr(0, kPos))] = TrimConfigToken(item.substr(kPos + 1));
            } else {
                AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidEnvEntry, "invalid environment entry '" + WideToUtf8(item) + "'");
            }
        }
    };

    handlers[L"exitcodes"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        std::vector<DWORD> parsedExitCodes;
        std::wstringstream ss(val);
        std::wstring token;
        while (std::getline(ss, token, L',')) {
            DWORD parsed = 0;
            std::wstring t = TrimConfigToken(token);
            if (TryParseDword(t, parsed)) {
                parsedExitCodes.push_back(parsed);
            } else {
                AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid exit code token '" + WideToUtf8(t) + "'");
            }
        }
        if (!parsedExitCodes.empty()) cfg.exitcodes = std::move(parsedExitCodes);
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "exitcodes list was empty or invalid (keeping default)");
    };

    return handlers;
}
