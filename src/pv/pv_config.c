#include "pv_internal.h"

void PvConfig_print_version(void) {
    printf("pv %s - Pipe Viewer \n", PV_VERSION);
    printf("Copyright (C) 2026, PC/OpenSystems LLC\n");
}

void PvConfig_print_help(void) {
    printf("Usage: pv [OPTIONS] [FILE]...\n");
    printf("Concatenate FILE(s) to standard output with monitoring.\n\n");
    printf("Display modifiers:\n");
    printf("  -p, --progress           turn progress bar on\n");
    printf("  -t, --timer              turn timer on\n");
    printf("  -e, --eta                turn ETA timer on\n");
    printf("  -I, --fineta             turn absolute ETA timer on\n");
    printf("  -r, --rate               turn rate counter on\n");
    printf("  -a, --average-rate       turn average rate counter on\n");
    printf("  -b, --bytes              turn byte counter on\n");
    printf("  -8, --bits               tell rate and bytes in bits\n");
    printf("  -T, --buffer-percent     turn buffer percent on\n");
    printf("  -A, --last-written NUM   show NUM bytes last written\n");
    printf("  -F, --format FORMAT      set output format\n");
    printf("  -n, --numeric            output numeric percentage only (0-100)\n");
    printf("  -q, --quiet              do not output any transfer information\n\n");
    printf("Output modifiers:\n");
    printf("  -W, --wait               wait until first byte transferred\n");
    printf("  -D, --delay-start SEC    delay start of display by SEC seconds\n");
    printf("  -s, --size SIZE          set total data size to SIZE bytes\n");
    printf("  -l, --line-mode          count lines instead of bytes\n");
    printf("  -0, --null               lines are null-terminated\n");
    printf("  -i, --interval SEC       update display every SEC seconds (default: 0.25)\n");
    printf("  -w, --width WIDTH        assume terminal width of WIDTH characters\n");
    printf("  -H, --height HEIGHT      assume terminal height of HEIGHT rows\n");
    printf("  -N, --name NAME          prefix visual display with NAME\n");
    printf("  -f, --force              force output even if stderr is not a terminal\n");
    printf("  -c, --cursor             use cursor positioning instead of carriage returns\n\n");
    printf("Data transfer modifiers:\n");
    printf("  -L, --rate-limit RATE    limit transfer to RATE bytes per second\n");
    printf("  -B, --buffer-size SIZE   set read buffer size to SIZE bytes (default: 64K)\n");
    printf("  -E, --skip-errors        skip read errors and continue\n");
    printf("  -S, --stop-at-size       stop transfer once SIZE bytes are read\n");
    printf("  -P, --pidfile FILE       save process PID to FILE\n\n");
    printf("General qualifiers:\n");
    printf("  -h, --help               display this help and exit\n");
    printf("  -V, --version            display version information and exit\n");
}

bool PvConfig_parse_args(PvConfig *self, int argc, char *argv[]) {
    bool explicit_display = false;

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] != '\0') {
            /* Long arguments */
            if (arg[1] == '-') {
                if (strcmp(arg, "--help") == 0) { self->print_help(); exit(0); }
                if (strcmp(arg, "--version") == 0) { self->print_version(); exit(0); }
                if (strcmp(arg, "--progress") == 0) { self->flag_progress = true; explicit_display = true; continue; }
                if (strcmp(arg, "--timer") == 0) { self->flag_timer = true; explicit_display = true; continue; }
                if (strcmp(arg, "--eta") == 0) { self->flag_eta = true; explicit_display = true; continue; }
                if (strcmp(arg, "--fineta") == 0) { self->flag_fineta = true; explicit_display = true; continue; }
                if (strcmp(arg, "--rate") == 0) { self->flag_rate = true; explicit_display = true; continue; }
                if (strcmp(arg, "--average-rate") == 0) { self->flag_average_rate = true; explicit_display = true; continue; }
                if (strcmp(arg, "--bytes") == 0) { self->flag_bytes = true; explicit_display = true; continue; }
                if (strcmp(arg, "--bits") == 0) { self->flag_bits = true; continue; }
                if (strcmp(arg, "--buffer-percent") == 0) { self->flag_bufpercent = true; continue; }
                if (strcmp(arg, "--numeric") == 0) { self->flag_numeric = true; continue; }
                if (strcmp(arg, "--quiet") == 0) { self->flag_quiet = true; continue; }
                if (strcmp(arg, "--wait") == 0) { self->flag_wait = true; continue; }
                if (strcmp(arg, "--line-mode") == 0) { self->flag_line_mode = true; continue; }
                if (strcmp(arg, "--null") == 0) { self->flag_null = true; continue; }
                if (strcmp(arg, "--force") == 0) { self->flag_force = true; continue; }
                if (strcmp(arg, "--cursor") == 0) { self->flag_cursor = true; continue; }
                if (strcmp(arg, "--skip-errors") == 0) { self->flag_skip_errors = true; continue; }
                if (strcmp(arg, "--stop-at-size") == 0) { self->flag_stop_at_size = true; continue; }
                if (strncmp(arg, "--size=", 7) == 0) { self->target_size = parse_size_suffix(arg + 7); continue; }
                if (strncmp(arg, "--rate-limit=", 13) == 0) { self->rate_limit = parse_size_suffix(arg + 13); continue; }
                if (strncmp(arg, "--buffer-size=", 14) == 0) { self->buffer_size = (size_t)parse_size_suffix(arg + 14); continue; }
                if (strncmp(arg, "--interval=", 11) == 0) { self->interval = atof(arg + 11); continue; }
                if (strncmp(arg, "--width=", 8) == 0) { self->term_width = atoi(arg + 8); continue; }
                if (strncmp(arg, "--name=", 7) == 0) { strncpy(self->name, arg + 7, sizeof(self->name) - 1); continue; }

                /* Long flags expecting value as next token */
                if (i + 1 < argc) {
                    if (strcmp(arg, "--size") == 0) { self->target_size = parse_size_suffix(argv[++i]); continue; }
                    if (strcmp(arg, "--rate-limit") == 0) { self->rate_limit = parse_size_suffix(argv[++i]); continue; }
                    if (strcmp(arg, "--buffer-size") == 0) { self->buffer_size = (size_t)parse_size_suffix(argv[++i]); continue; }
                    if (strcmp(arg, "--interval") == 0) { self->interval = atof(argv[++i]); continue; }
                    if (strcmp(arg, "--width") == 0) { self->term_width = atoi(argv[++i]); continue; }
                    if (strcmp(arg, "--name") == 0) { strncpy(self->name, argv[++i], sizeof(self->name) - 1); continue; }
                    if (strcmp(arg, "--pidfile") == 0) { strncpy(self->pidfile, argv[++i], sizeof(self->pidfile) - 1); continue; }
                }
                fprintf(stderr, "pv: unrecognized option '%s'\nTry 'pv --help' for more information.\n", arg);
                return false;
            }

            /* Short flags (grouped, e.g. -peta) */
            for (size_t c = 1; c < strlen(arg); c++) {
                switch (arg[c]) {
                    case 'h': self->print_help(); exit(0);
                    case 'V': self->print_version(); exit(0);
                    case 'p': self->flag_progress = true; explicit_display = true; break;
                    case 't': self->flag_timer = true; explicit_display = true; break;
                    case 'e': self->flag_eta = true; explicit_display = true; break;
                    case 'I': self->flag_fineta = true; explicit_display = true; break;
                    case 'r': self->flag_rate = true; explicit_display = true; break;
                    case 'a': self->flag_average_rate = true; explicit_display = true; break;
                    case 'b': self->flag_bytes = true; explicit_display = true; break;
                    case '8': self->flag_bits = true; break;
                    case 'T': self->flag_bufpercent = true; break;
                    case 'n': self->flag_numeric = true; break;
                    case 'q': self->flag_quiet = true; break;
                    case 'W': self->flag_wait = true; break;
                    case 'l': self->flag_line_mode = true; break;
                    case '0': self->flag_null = true; break;
                    case 'f': self->flag_force = true; break;
                    case 'c': self->flag_cursor = true; break;
                    case 'E': self->flag_skip_errors = true; break;
                    case 'S': self->flag_stop_at_size = true; break;
                    /* Short flags requiring an argument */
                    case 's':
                    case 'L':
                    case 'B':
                    case 'i':
                    case 'w':
                    case 'H':
                    case 'N':
                    case 'D':
                    case 'P': {
                        char opt = arg[c];
                        const char *optval = NULL;
                        if (arg[c + 1] != '\0') {
                            optval = &arg[c + 1];
                            c = strlen(arg) - 1;
                        } else if (i + 1 < argc) {
                            optval = argv[++i];
                        } else {
                            fprintf(stderr, "pv: option '-%c' requires an argument\n", opt);
                            return false;
                        }
                        if (opt == 's') self->target_size = parse_size_suffix(optval);
                        else if (opt == 'L') self->rate_limit = parse_size_suffix(optval);
                        else if (opt == 'B') self->buffer_size = (size_t)parse_size_suffix(optval);
                        else if (opt == 'i') self->interval = atof(optval);
                        else if (opt == 'w') self->term_width = atoi(optval);
                        else if (opt == 'H') self->term_height = atoi(optval);
                        else if (opt == 'N') strncpy(self->name, optval, sizeof(self->name) - 1);
                        else if (opt == 'D') self->delay_start = atof(optval);
                        else if (opt == 'P') strncpy(self->pidfile, optval, sizeof(self->pidfile) - 1);
                        break;
                    }
                    default:
                        fprintf(stderr, "pv: invalid option -- '%c'\nTry 'pv --help' for more information.\n", arg[c]);
                        return false;
                }
            }
        } else {
            /* File argument */
            self->input_files = &argv[i];
            self->input_file_count = argc - i;
            break;
        }
    }

    /* Default modifiers if none were explicitly turned on */
    if (!explicit_display && !self->flag_numeric) {
        self->flag_bytes = true;
        self->flag_timer = true;
        self->flag_rate = true;
        self->flag_progress = true;
        self->flag_eta = true;
    }

    return true;
}

PvConfig *PvConfig_New(void) {
    PvConfig *self = (PvConfig *)calloc(1, sizeof(PvConfig));
    self->interval = DEFAULT_INTERVAL;
    self->buffer_size = DEFAULT_BUFFER_SIZE;
    self->print_help = PvConfig_print_help;
    self->print_version = PvConfig_print_version;
    self->parse_args = PvConfig_parse_args;
    return self;
}
