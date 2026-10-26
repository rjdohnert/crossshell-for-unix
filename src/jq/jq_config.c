#include "jq_internal.h"

void JqConfig_print_version(void) {
    printf("jq\n", JQ_VERSION);
}

void JqConfig_print_help(void) {
    printf("jq - Command-line JSON processor [version %s]\n\n", JQ_VERSION);
    printf("Usage: jq [options] <jq filter> [file...]\n");
    printf("       jq [options] --args <jq filter> [strings...]\n");
    printf("       jq [options] --jsonargs <jq filter> [json_strings...]\n\n");
    printf("Basic options:\n");
    printf("  -c, --compact-output    compact instead of pretty-printed output\n");
    printf("  -n, --null-input        use `null` as the single input value\n");
    printf("  -e, --exit-status       set exit status code based on output truthiness\n");
    printf("  -s, --slurp             read entire input stream into large array\n");
    printf("  -r, --raw-output        output raw strings, not JSON quoted\n");
    printf("  -j, --join-output       like -r, but do not emit a newline after each output\n");
    printf("  -R, --raw-input         read input lines as strings instead of parsing JSON\n");
    printf("  -S, --sort-keys         sort keys of objects on output\n");
    printf("  -C, --color-output      colorize JSON (default on terminal)\n");
    printf("  -M, --monochrome-output disable colorization\n");
    printf("      --tab               use tabs for indentation\n");
    printf("      --indent <n>        use n spaces for indentation (default: 2)\n");
    printf("      --unbuffered        flush output stream after each JSON object\n");
    printf("      --stream            parse input in streaming mode\n");
    printf("      --seq               use application/json-seq format\n");
    printf("  -a, --ascii-output      escape non-ASCII characters to \\uXXXX\n\n");
    printf("Filter source & libraries:\n");
    printf("  -f, --from-file <file>  read filter from file instead of command line\n");
    printf("  -L <dir>                prepend directory to library search path\n\n");
    printf("Variables & Parameters:\n");
    printf("  --arg <v> <val>         set $v to string value <val>\n");
    printf("  --argjson <v> <val>     set $v to parsed JSON value <val>\n");
    printf("  --slurpfile <v> <file>  set $v to array of JSON values read from <file>\n");
    printf("  --rawfile <v> <file>    set $v to string containing content of <file>\n");
    printf("  --args                  consume remaining arguments as positional string array\n");
    printf("  --jsonargs              consume remaining arguments as positional JSON values\n\n");
    printf("Informational:\n");
    printf("  --run-tests             run integrated test harness\n");
    printf("  -V, --version           show version and exit\n");
    printf("  -h, --help              show this help message and exit\n");
}

bool JqConfig_parse_args(JqConfig *self, int argc, char *argv[]) {
    int i = 1;
    bool filter_seen = false;

    while (i < argc) {
        char *arg = argv[i];

        if (arg[0] == '-' && arg[1] != '\0') {
            if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
                self->print_help();
                exit(0);
            }
            if (strcmp(arg, "--version") == 0 || strcmp(arg, "-V") == 0) {
                self->print_version();
                exit(0);
            }
            if (strcmp(arg, "-c") == 0 || strcmp(arg, "--compact-output") == 0) {
                self->compact_output = true;
            } else if (strcmp(arg, "-n") == 0 || strcmp(arg, "--null-input") == 0) {
                self->null_input = true;
            } else if (strcmp(arg, "-r") == 0 || strcmp(arg, "--raw-output") == 0) {
                self->raw_output = true;
            } else if (strcmp(arg, "-j") == 0 || strcmp(arg, "--join-output") == 0) {
                self->join_output = true;
                self->raw_output = true;
            } else if (strcmp(arg, "-R") == 0 || strcmp(arg, "--raw-input") == 0) {
                self->raw_input = true;
            } else if (strcmp(arg, "-s") == 0 || strcmp(arg, "--slurp") == 0) {
                self->slurp = true;
            } else if (strcmp(arg, "-S") == 0 || strcmp(arg, "--sort-keys") == 0) {
                self->sort_keys = true;
            } else if (strcmp(arg, "-C") == 0 || strcmp(arg, "--color-output") == 0) {
                self->color_output = true;
                self->monochrome_output = false;
            } else if (strcmp(arg, "-M") == 0 || strcmp(arg, "--monochrome-output") == 0) {
                self->monochrome_output = true;
                self->color_output = false;
            } else if (strcmp(arg, "-a") == 0 || strcmp(arg, "--ascii-output") == 0) {
                self->ascii_output = true;
            } else if (strcmp(arg, "-e") == 0 || strcmp(arg, "--exit-status") == 0) {
                self->exit_status = true;
            } else if (strcmp(arg, "--tab") == 0) {
                self->tab_indent = true;
            } else if (strcmp(arg, "--unbuffered") == 0) {
                self->unbuffered = true;
            } else if (strcmp(arg, "--stream") == 0) {
                self->stream = true;
            } else if (strcmp(arg, "--seq") == 0) {
                self->seq = true;
            } else if (strcmp(arg, "--run-tests") == 0) {
                self->run_tests = true;
            } else if (strcmp(arg, "--indent") == 0 && i + 1 < argc) {
                self->indent_spaces = atoi(argv[++i]);
            } else if ((strcmp(arg, "-f") == 0 || strcmp(arg, "--from-file") == 0) && i + 1 < argc) {
                strncpy(self->from_file, argv[++i], MAX_PATH - 1);
            } else if (strcmp(arg, "-L") == 0 && i + 1 < argc) {
                strncpy(self->library_path, argv[++i], MAX_PATH - 1);
            } else if (strcmp(arg, "--arg") == 0 && i + 2 < argc) {
                char *name = argv[++i];
                char *val  = argv[++i];
                if (self->named_arg_count < 64) {
                    strncpy(self->named_args[self->named_arg_count].name, name, 127);
                    self->named_args[self->named_arg_count].val = JqValue_NewString(val);
                    self->named_arg_count++;
                }
            } else if (strcmp(arg, "--argjson") == 0 && i + 2 < argc) {
                char *name = argv[++i];
                char *val  = argv[++i];
                /* Parsed dynamically via JqParser later */
                if (self->named_arg_count < 64) {
                    strncpy(self->named_args[self->named_arg_count].name, name, 127);
                    self->named_args[self->named_arg_count].val = JqValue_NewString(val);
                    self->named_arg_count++;
                }
            } else if (strcmp(arg, "--args") == 0) {
                self->args_mode = true;
                i++;
                break;
            } else if (strcmp(arg, "--jsonargs") == 0) {
                self->jsonargs_mode = true;
                i++;
                break;
            } else {
                /* Bundled short flags, e.g., -cr */
                size_t len = strlen(arg);
                for (size_t c = 1; c < len; c++) {
                    switch (arg[c]) {
                        case 'c': self->compact_output = true; break;
                        case 'n': self->null_input = true; break;
                        case 'r': self->raw_output = true; break;
                        case 'j': self->join_output = true; self->raw_output = true; break;
                        case 'R': self->raw_input = true; break;
                        case 's': self->slurp = true; break;
                        case 'S': self->sort_keys = true; break;
                        case 'C': self->color_output = true; self->monochrome_output = false; break;
                        case 'M': self->monochrome_output = true; self->color_output = false; break;
                        case 'a': self->ascii_output = true; break;
                        case 'e': self->exit_status = true; break;
                        case 'h': self->print_help(); exit(0);
                        case 'V': self->print_version(); exit(0);
                        default:
                            fprintf(stderr, "jq: Unknown option %s\nUse jq --help for list of options\n", arg);
                            return false;
                    }
                }
            }
        } else {
            /* Positional parameter */
            if (!filter_seen && self->from_file[0] == '\0') {
                self->filter_string = _strdup(arg);
                filter_seen = true;
            } else {
                self->input_files = &argv[i];
                self->input_file_count = argc - i;
                break;
            }
        }
        i++;
    }

    if (!self->filter_string && self->from_file[0] == '\0') {
        self->filter_string = _strdup("."); /* Default identity filter */
    }

    return true;
}

JqConfig *JqConfig_New(void) {
    JqConfig *c = (JqConfig *)calloc(1, sizeof(JqConfig));
    c->indent_spaces = 2;
    c->print_help = JqConfig_print_help;
    c->print_version = JqConfig_print_version;
    c->parse_args = JqConfig_parse_args;
    return c;
}
