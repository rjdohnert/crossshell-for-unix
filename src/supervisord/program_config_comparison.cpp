#include "program_config_comparison.hpp"
#include "program_config.hpp"

bool ProgramConfigEquals(const ProgramConfig& a, const ProgramConfig& b) {
    return a.name == b.name &&
           a.command == b.command &&
           a.directory == b.directory &&
           a.depends_on == b.depends_on &&
           a.shutdown_phase == b.shutdown_phase &&
           a.autostart == b.autostart &&
           a.autorestart == b.autorestart &&
           a.exitcodes == b.exitcodes &&
           a.startretries == b.startretries &&
           a.stoptimeout == b.stoptimeout &&
           a.stop_command == b.stop_command &&
           a.healthcheck_command == b.healthcheck_command &&
           a.healthcheck_interval == b.healthcheck_interval &&
           a.healthcheck_failures == b.healthcheck_failures &&
           a.stdout_logfile == b.stdout_logfile &&
           a.stdout_logfile_maxbytes == b.stdout_logfile_maxbytes &&
           a.stdout_logfile_backups == b.stdout_logfile_backups &&
           a.stderr_logfile == b.stderr_logfile &&
           a.stderr_logfile_maxbytes == b.stderr_logfile_maxbytes &&
           a.stderr_logfile_backups == b.stderr_logfile_backups &&
           a.environment == b.environment;
}
