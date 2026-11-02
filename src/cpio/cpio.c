/**
 * CPIO (CrossShell for UNIX)
 * Copyright (C) 2026, PC/OpenSystems LC
 *
 * Licensed under the terms of the BSD-3 License
 */

#include "cpio.h"
#include "cpio_cli.h"
#include "cpio_engine.h"
#include "cpio_path.h"

/* Global state for POSIX exit codes and thread-safe signal cleanup */
int                  g_ExitStatus = 0;
volatile LONG        g_AbortRequested = 0;
UINT                 g_OriginalConsoleCP = 0;
UINT                 g_OriginalConsoleOutputCP = 0;

void SetWarning(void) {
    if (g_ExitStatus == 0) g_ExitStatus = 1;
}

void SetFatal(void) {
    g_ExitStatus = 2;
}

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    (void)ctrlType;
    InterlockedExchange(&g_AbortRequested, 1);
    return TRUE;
}

int main(void) {
    g_OriginalConsoleCP = GetConsoleCP();
    g_OriginalConsoleOutputCP = GetConsoleOutputCP();
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    int wargc = 0;
    wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (!wargv || wargc < 2) {
        DisplayHelp();
        if (wargv) LocalFree(wargv);
        return 2;
    }

    char** argv = (char**)malloc(wargc * sizeof(char*));
    if (!argv) {
        LocalFree(wargv);
        return 2;
    }
    for (int idx = 0; idx < wargc; ++idx) {
        argv[idx] = WideToUtf8(wargv[idx]);
    }
    LocalFree(wargv);

    CpioConfig cfg = {0};
    cfg.mode = MODE_NONE;
    cfg.createFormat = FMT_NEWC;
    cfg.patternCapacity = 16;
    cfg.patterns = (char**)malloc(cfg.patternCapacity * sizeof(char*));
    if (!cfg.patterns) {
        for (int idx = 0; idx < wargc; ++idx) free(argv[idx]);
        free(argv);
        return 2;
    }

    for (int i = 1; i < wargc; ++i) {
        const char* arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-?") == 0) {
            DisplayHelp();
            CleanupConfig(&cfg);
            for (int idx = 0; idx < wargc; ++idx) free(argv[idx]);
            free(argv);
            return 0;
        }
        if (strcmp(arg, "--version") == 0) {
            puts("CPIO 8.0.0");
            CleanupConfig(&cfg);
            for (int idx = 0; idx < wargc; ++idx) free(argv[idx]);
            free(argv);
            return 0;
        }
        if (strcmp(arg, "--create") == 0) { cfg.mode = MODE_CREATE; continue; }
        if (strcmp(arg, "--extract") == 0) { cfg.mode = MODE_EXTRACT; continue; }
        if (strcmp(arg, "--list") == 0) { cfg.mode = MODE_LIST; continue; }
        if (strcmp(arg, "--verbose") == 0) { cfg.verbose = true; continue; }
        if (strcmp(arg, "--make-directories") == 0) { cfg.makeDirs = true; continue; }
        if (strcmp(arg, "--unconditional") == 0) { cfg.unconditional = true; continue; }
        if (strcmp(arg, "--preserve-mtime") == 0) { cfg.preserveTime = true; continue; }
        if (strcmp(arg, "--null") == 0) { cfg.nullTerminated = true; continue; }

        if (strncmp(arg, "--file=", 7) == 0) {
            if (cfg.archivePath) free(cfg.archivePath);
            cfg.archivePath = _strdup(arg + 7);
            continue;
        }
        if (strcmp(arg, "--file") == 0) {
            if (i + 1 < wargc) {
                if (cfg.archivePath) free(cfg.archivePath);
                cfg.archivePath = _strdup(argv[++i]);
            } else {
                fprintf(stderr, "cpio: error: --file requires an argument\n");
                CleanupConfig(&cfg);
                for (int idx = 0; idx < wargc; ++idx) free(argv[idx]);
                free(argv);
                return 2;
            }
            continue;
        }

        if (strncmp(arg, "--format=", 9) == 0) {
            const char* fmtStr = arg + 9;
            if (_stricmp(fmtStr, "newc") == 0) cfg.createFormat = FMT_NEWC;
            else if (_stricmp(fmtStr, "crc") == 0) cfg.createFormat = FMT_CRC;
            else if (_stricmp(fmtStr, "odc") == 0) cfg.createFormat = FMT_ODC;
            else {
                fprintf(stderr, "cpio: unsupported format '%s'\n", fmtStr);
                CleanupConfig(&cfg);
                for (int idx = 0; idx < wargc; ++idx) free(argv[idx]);
                free(argv);
                return 2;
            }
            continue;
        }

        if (arg[0] == '-' && arg[1] != '\0' && arg[1] != '-') {
            for (int j = 1; arg[j]; ++j) {
                switch (arg[j]) {
                    case 'o': cfg.mode = MODE_CREATE; break;
                    case 'i': cfg.mode = MODE_EXTRACT; break;
                    case 't': cfg.mode = MODE_LIST; break;
                    case 'p': cfg.mode = MODE_PASSTHROUGH; break;
                    case 'v': cfg.verbose = true; break;
                    case 'd': cfg.makeDirs = true; break;
                    case 'u': cfg.unconditional = true; break;
                    case 'm': cfg.preserveTime = true; break;
                    case '0': cfg.nullTerminated = true; break;
                    case 'B': cfg.blockFactor5120 = true; break;
                    case 'H': {
                        const char* fmtStr = NULL;
                        if (arg[j + 1] != '\0') {
                            fmtStr = &arg[j + 1];
                            j += (int)strlen(&arg[j + 1]) - 1;
                        } else if (i + 1 < wargc) {
                            fmtStr = argv[++i];
                        } else {
                            fprintf(stderr, "cpio: error: -H requires an argument\n");
                            CleanupConfig(&cfg);
                            for (int idx = 0; idx < wargc; ++idx) free(argv[idx]);
                            free(argv);
                            return 2;
                        }
                        if (_stricmp(fmtStr, "newc") == 0) cfg.createFormat = FMT_NEWC;
                        else if (_stricmp(fmtStr, "crc") == 0) cfg.createFormat = FMT_CRC;
                        else if (_stricmp(fmtStr, "odc") == 0) cfg.createFormat = FMT_ODC;
                        else {
                            fprintf(stderr, "cpio: unsupported format: '%s'\n", fmtStr);
                            CleanupConfig(&cfg);
                            for (int idx = 0; idx < wargc; ++idx) free(argv[idx]);
                            free(argv);
                            return 2;
                        }
                        break;
                    }
                    case 'F':
                        if (arg[j + 1] != '\0') {
                            if (cfg.archivePath) free(cfg.archivePath);
                            cfg.archivePath = _strdup(&arg[j + 1]);
                            j += (int)strlen(&arg[j + 1]) - 1;
                        } else if (i + 1 < wargc) {
                            if (cfg.archivePath) free(cfg.archivePath);
                            cfg.archivePath = _strdup(argv[++i]);
                        } else {
                            fprintf(stderr, "cpio: error: -F expects an archive path\n");
                            CleanupConfig(&cfg);
                            for (int idx = 0; idx < wargc; ++idx) free(argv[idx]);
                            free(argv);
                            return 2;
                        }
                        break;
                    default:
                        fprintf(stderr, "cpio: unknown modifier: '-%c'\n", arg[j]);
                        CleanupConfig(&cfg);
                        for (int idx = 0; idx < wargc; ++idx) free(argv[idx]);
                        free(argv);
                        return 2;
                }
            }
            continue;
        }

        /* Positional parameters */
        if (cfg.mode == MODE_CREATE) {
            fprintf(stderr, "cpio: unexpected argument '%s' (use -F to specify archive file)\n", arg);
            CleanupConfig(&cfg);
            for (int idx = 0; idx < wargc; ++idx) free(argv[idx]);
            free(argv);
            return 2;
        } else if (cfg.mode == MODE_PASSTHROUGH && cfg.targetDir == NULL) {
            char* td = _strdup(arg);
            NormalizeSlashes(td);
            size_t tlen = strlen(td);
            while (tlen > 1 && (td[tlen - 1] == '/' || td[tlen - 1] == '\\')) {
                td[--tlen] = '\0';
            }
            cfg.targetDir = td;
        } else if (cfg.mode == MODE_EXTRACT || cfg.mode == MODE_LIST) {
            if (cfg.patternCount >= cfg.patternCapacity) {
                cfg.patternCapacity *= 2;
                char** np = (char**)realloc(cfg.patterns, cfg.patternCapacity * sizeof(char*));
                if (!np) {
                    CleanupConfig(&cfg);
                    for (int idx = 0; idx < wargc; ++idx) free(argv[idx]);
                    free(argv);
                    return 2;
                }
                cfg.patterns = np;
            }
            cfg.patterns[cfg.patternCount++] = _strdup(arg);
        }
    }

    if (cfg.mode == MODE_NONE) {
        fprintf(stderr, "cpio: operational mode required (-o, -i, -t, or -p)\n");
        CleanupConfig(&cfg);
        for (int idx = 0; idx < wargc; ++idx) free(argv[idx]);
        free(argv);
        return 2;
    }

    if (cfg.mode == MODE_PASSTHROUGH && cfg.targetDir == NULL) {
        fprintf(stderr, "cpio: pass-through mode (-p) requires a destination directory\n");
        CleanupConfig(&cfg);
        for (int idx = 0; idx < wargc; ++idx) free(argv[idx]);
        free(argv);
        return 2;
    }

    int result = 0;
    switch (cfg.mode) {
        case MODE_CREATE:      result = ExecuteCreate(&cfg); break;
        case MODE_EXTRACT:
        case MODE_LIST:        result = ExecuteExtractOrList(&cfg); break;
        case MODE_PASSTHROUGH: result = ExecutePassThrough(&cfg); break;
        default: result = 2; break;
    }

    CleanupConfig(&cfg);
    for (int idx = 0; idx < wargc; ++idx) free(argv[idx]);
    free(argv);

    SetConsoleCP(g_OriginalConsoleCP);
    SetConsoleOutputCP(g_OriginalConsoleOutputCP);
    return result;
}
