#include "zstd.h"

/* =========================================================================
 * Global Process State & Console Control
 * ========================================================================= */

CRITICAL_SECTION g_cleanup_lock;
char g_active_output_file[MAX_PATH_EXTENDED] = "";
static volatile LONG g_is_cleaning = 0;

static BOOL WINAPI console_ctrl_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        if (InterlockedExchange(&g_is_cleaning, 1) == 0) {
            EnterCriticalSection(&g_cleanup_lock);
            if (g_active_output_file[0] != '\0') {
                DeleteFileA(g_active_output_file);
                g_active_output_file[0] = '\0';
            }
            LeaveCriticalSection(&g_cleanup_lock);
        }
        ExitProcess(1);
    }
    return FALSE;
}

/* =========================================================================
 * MODULE: ZstdConfig (Command Line Qualifiers & Parser)
 * ========================================================================= */

static void ZstdConfig_print_version(void) {
    printf("*** zstd command line interface 64-bits %s, by PC/OpenSystems LLC ***\n", ZSTD_CLI_VERSION);
}

static void ZstdConfig_print_help(void) {
    printf("Usage: zstd [OPTIONS] [-|INPUT-FILE] [-o OUTPUT-FILE]\n\n");
    printf("Operation modes:\n");
    printf("  -z, --compress            Force compression (default)\n");
    printf("  -d, --decompress          Decompress\n");
    printf("  -t, --test                Test compressed file integrity\n");
    printf("  -l, --list                Display information about compressed file(s)\n");
    printf("  -b#                       Benchmark file(s) at compression level #\n\n");
    printf("Compression parameters:\n");
    printf("  -#                        Compression level (1-19, default: 3)\n");
    printf("      --ultra               Unlock levels 20-22 (requires more memory)\n");
    printf("      --fast[=#]            Ultra-fast compression mode\n");
    printf("  -T#, --threads=#          Number of worker threads (0: autodetect all CPUs across all processor groups)\n\n");
    printf("File and I/O options:\n");
    printf("  -o FILE                   Save output into FILE (supports extended paths > 260 chars)\n");
    printf("  -c, --stdout              Write to standard output\n");
    printf("  -f, --force               Overwrite output without prompting\n");
    printf("  -k, --keep                Preserve source file(s) (default)\n");
    printf("      --rm                  Delete source file(s) on success\n\n");
    printf("Display & Format options:\n");
    printf("  -v, --verbose             Verbose mode, display additional status\n");
    printf("  -q, --quiet               Suppress warnings and normal output\n");
    printf("      --no-progress         Do not display continuous terminal progress\n");
    printf("  -C, --check               Add 32-bit xxHash checksum (RFC 8878 XXH64 lower 32-bits, default)\n");
    printf("      --no-check            Disable frame checksum\n");
    printf("      --content-size        Record original uncompressed size (default)\n");
    printf("      --no-content-size     Do not record original size\n\n");
    printf("General qualifiers:\n");
    printf("  -h, -H, --help            Show this comprehensive help and exit\n");
    printf("  -V, --version             Show version and exit\n");
}

static bool ZstdConfig_parse(ZstdConfig *self, int argc, char *argv[]) {
    self->input_files = (char **)calloc(argc + 1, sizeof(char *));
    self->input_file_count = 0;

    int i = 1;
    while (i < argc) {
        char *arg = argv[i];
        if (strcmp(arg, "--") == 0) {
            i++;
            while (i < argc) {
                self->input_files[self->input_file_count++] = argv[i++];
            }
            break;
        }
        if (arg[0] == '-' && arg[1] != '\0') {
            if (arg[1] == '-') {
                if (strcmp(arg, "--help") == 0) { self->print_help(); exit(0); }
                if (strcmp(arg, "--version") == 0) { self->print_version(); exit(0); }
                if (strcmp(arg, "--compress") == 0) { self->mode = MODE_COMPRESS; }
                else if (strcmp(arg, "--decompress") == 0 || strcmp(arg, "--uncompress") == 0) { self->mode = MODE_DECOMPRESS; }
                else if (strcmp(arg, "--test") == 0) { self->mode = MODE_TEST; }
                else if (strcmp(arg, "--list") == 0) { self->mode = MODE_LIST; }
                else if (strcmp(arg, "--ultra") == 0) { self->ultra = true; }
                else if (strcmp(arg, "--force") == 0) { self->force = true; }
                else if (strcmp(arg, "--keep") == 0) { self->keep = true; }
                else if (strcmp(arg, "--rm") == 0) { self->rm_source = true; self->keep = false; }
                else if (strcmp(arg, "--stdout") == 0) { self->to_stdout = true; }
                else if (strcmp(arg, "--quiet") == 0) { self->quiet = true; }
                else if (strcmp(arg, "--verbose") == 0) { self->verbose++; }
                else if (strcmp(arg, "--no-progress") == 0) { self->no_progress = true; }
                else if (strcmp(arg, "--check") == 0) { self->checksum = true; }
                else if (strcmp(arg, "--no-check") == 0) { self->checksum = false; }
                else if (strcmp(arg, "--content-size") == 0) { self->content_size = true; }
                else if (strcmp(arg, "--no-content-size") == 0) { self->content_size = false; }
                else if (strncmp(arg, "--threads=", 10) == 0) { self->threads = atoi(arg + 10); }
                else if (strncmp(arg, "--fast=", 7) == 0) { self->fast_level = atoi(arg + 7); }
                else if (strcmp(arg, "--fast") == 0) { self->fast_level = 1; }
                else if (strncmp(arg, "--output=", 9) == 0) { strncpy(self->output_file, arg + 9, MAX_PATH_EXTENDED - 1); }
                else {
                    fprintf(stderr, "zstd: unknown option '%s'\n", arg);
                    return false;
                }
            } else {
                size_t len = strlen(arg);
                for (size_t c = 1; c < len; c++) {
                    char opt = arg[c];
                    if (isdigit((unsigned char)opt)) {
                        self->level = atoi(&arg[c]);
                        break;
                    }
                    switch (opt) {
                        case 'h': case 'H': self->print_help(); exit(0);
                        case 'V': self->print_version(); exit(0);
                        case 'z': self->mode = MODE_COMPRESS; break;
                        case 'd': self->mode = MODE_DECOMPRESS; break;
                        case 't': self->mode = MODE_TEST; break;
                        case 'l': self->mode = MODE_LIST; break;
                        case 'f': self->force = true; break;
                        case 'k': self->keep = true; break;
                        case 'c': self->to_stdout = true; break;
                        case 'C': self->checksum = true; break;
                        case 'q': self->quiet = true; break;
                        case 'v': self->verbose++; break;
                        case 'b':
                            self->mode = MODE_BENCHMARK;
                            if (arg[c + 1] != '\0') {
                                self->level = atoi(&arg[c + 1]);
                                c = len - 1;
                            }
                            break;
                        case 'T':
                            if (arg[c + 1] != '\0') {
                                self->threads = atoi(&arg[c + 1]);
                                c = len - 1;
                            } else if (i + 1 < argc) {
                                self->threads = atoi(argv[++i]);
                            }
                            break;
                        case 'o':
                            if (i + 1 < argc) strncpy(self->output_file, argv[++i], MAX_PATH_EXTENDED - 1);
                            c = len - 1;
                            break;
                        default:
                            fprintf(stderr, "zstd: invalid option -- '%c'\n", opt);
                            return false;
                    }
                }
            }
        } else {
            self->input_files[self->input_file_count++] = arg;
        }
        i++;
    }

    if (self->level > 19 && !self->ultra) {
        fprintf(stderr, "zstd: compression level %d requires --ultra flag\n", self->level);
        return false;
    }
    if (self->level > 22) self->level = 22;

    if (self->threads <= 0) {
        self->threads = detect_processor_count();
        if (self->threads < 1) self->threads = 1;
    }

    return true;
}

ZstdConfig *ZstdConfig_New(void) {
    ZstdConfig *self = (ZstdConfig *)calloc(1, sizeof(ZstdConfig));
    self->mode = MODE_COMPRESS;
    self->level = 3;
    self->threads = 1;
    self->checksum = true;
    self->content_size = true;
    self->keep = true;
    self->print_help = ZstdConfig_print_help;
    self->print_version = ZstdConfig_print_version;
    self->parse = ZstdConfig_parse;
    return self;
}

/* =========================================================================
 * Application Entry Point
 * ========================================================================= */

int main(int argc, char *argv[]) {
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    InitializeCriticalSection(&g_cleanup_lock);
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    ZstdConfig *config = ZstdConfig_New();
    if (!config->parse(config, argc, argv)) {
        free(config);
        DeleteCriticalSection(&g_cleanup_lock);
        return 1;
    }

    ZstdEngine *engine = ZstdEngine_New(config);
    int status = engine->run(engine);

    free(engine);
    if (config->input_files) free(config->input_files);
    free(config);
    DeleteCriticalSection(&g_cleanup_lock);
    return status;
}