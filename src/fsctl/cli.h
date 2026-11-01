#ifndef FSCTL_CLI_H
#define FSCTL_CLI_H

void print_cli_help(const wchar_t *prog_name);

// Parses command-line qualifiers into g_app.
// Returns -1 to continue running, or a process exit code to exit immediately.
int cli_parse_args(int argc, wchar_t *argv[]);

#endif // FSCTL_CLI_H
