/**
 * ============================================================================
 * Cat (CrossShell for UNIX)
 * Copyright (C) 2026, PCX/OpenSystems LLC
 * Licensed under the terms and conditions of the BSD-3 Clause license
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <io.h>
#include <fcntl.h>

#define CAT_VERSION "1.0.0"
#define CAT_PROGRAM_NAME "cat"

/* ============================================================================
 * CLASS: CatOptions
 * Encapsulates CLI argument parsing, flags, and configuration.
 * ============================================================================ */
typedef struct CatOptions CatOptions;

struct CatOptions {
    /* State */
    bool number_nonblank;   /* -b: Number non-empty output lines, overrides -n */
    bool show_ends;         /* -E: Display $ at end of each line */
    bool number_all;        /* -n: Number all output lines */
    bool squeeze_blank;     /* -s: Suppress repeated empty output lines */
    bool show_tabs;         /* -T: Display TAB characters as ^I */
    bool show_nonprinting;  /* -v: Use ^ and M- notation, except LFD and TAB */
    bool unbuffered;        /* -u: Unbuffered output */
    
    char** files;           /* Input files list */
    int    file_count;
    int    file_capacity;

    /* Methods */
    bool (*Parse)(CatOptions* this, int argc, char** argv);
    void (*PrintHelp)(const CatOptions* this);
    void (*PrintVersion)(const CatOptions* this);
    void (*AddFile)(CatOptions* this, const char* filename);
};

/* Forward declarations */
static bool CatOptions_Parse(CatOptions* this, int argc, char** argv);
static void CatOptions_PrintHelp(const CatOptions* this);
static void CatOptions_PrintVersion(const CatOptions* this);
static void CatOptions_AddFile(CatOptions* this, const char* filename);

CatOptions* New_CatOptions(void) {
    CatOptions* opts = (CatOptions*)malloc(sizeof(CatOptions));
    if (!opts) return NULL;

    opts->number_nonblank  = false;
    opts->show_ends        = false;
    opts->number_all       = false;
    opts->squeeze_blank    = false;
    opts->show_tabs        = false;
    opts->show_nonprinting = false;
    opts->unbuffered       = false;

    opts->file_count       = 0;
    opts->file_capacity    = 16;
    opts->files            = (char**)malloc(sizeof(char*) * opts->file_capacity);

    /* Method binding */
    opts->Parse        = CatOptions_Parse;
    opts->PrintHelp    = CatOptions_PrintHelp;
    opts->PrintVersion = CatOptions_PrintVersion;
    opts->AddFile      = CatOptions_AddFile;

    return opts;
}

void Delete_CatOptions(CatOptions* this) {
    if (!this) return;
    for (int i = 0; i < this->file_count; i++) {
        free(this->files[i]);
    }
    free(this->files);
    free(this);
}

static void CatOptions_AddFile(CatOptions* this, const char* filename) {
    if (this->file_count >= this->file_capacity) {
        this->file_capacity *= 2;
        this->files = (char**)realloc(this->files, sizeof(char*) * this->file_capacity);
    }
    this->files[this->file_count] = _strdup(filename);
    this->file_count++;
}

static void CatOptions_PrintHelp(const CatOptions* this) {
    (void)this;
    printf(
        "Usage: %s [OPTION]... [FILE]...\n"
        "Concatenate FILE(s) to standard output.\n\n"
        "With no FILE, or when FILE is -, read standard input.\n\n"
        "  -A, --show-all           equivalent to -vET\n"
        "  -b, --number-nonblank    number nonempty output lines, overrides -n\n"
        "  -e                       equivalent to -vE\n"
        "  -E, --show-ends          display $ at end of each line\n"
        "  -n, --number             number all output lines\n"
        "  -s, --squeeze-blank      suppress repeated empty output lines\n"
        "  -t                       equivalent to -vT\n"
        "  -T, --show-tabs          display TAB characters as ^I\n"
        "  -u                       unbuffered output (disable stdio buffering)\n"
        "  -v, --show-nonprinting   use ^ and M- notation, except for LFD and TAB\n"
        "      --help               display this help and exit\n"
        "      --version            output version information and exit\n\n"
        "Examples:\n"
        "  %s f - g  Output f's contents, then standard input, then g's contents.\n"
        "  %s        Copy standard input to standard output.\n",
        CAT_PROGRAM_NAME, CAT_PROGRAM_NAME, CAT_PROGRAM_NAME
    );
}

static void CatOptions_PrintVersion(const CatOptions* this) {
    (void)this;
    printf("%s %s\n", CAT_PROGRAM_NAME, CAT_VERSION);
    printf("License: MIT. Written in Object-Oriented C for Windows.\n");
}

static bool CatOptions_Parse(CatOptions* this, int argc, char** argv) {
    bool end_of_options = false;
    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];

        if (end_of_options) {
            this->AddFile(this, arg);
            continue;
        }

        if (strcmp(arg, "--") == 0) {
            end_of_options = true;
        } else if (strcmp(arg, "--help") == 0) {
            this->PrintHelp(this);
            exit(EXIT_SUCCESS);
        } else if (strcmp(arg, "--version") == 0) {
            this->PrintVersion(this);
            exit(EXIT_SUCCESS);
        } else if (strcmp(arg, "--show-all") == 0) {
            this->show_nonprinting = true;
            this->show_ends = true;
            this->show_tabs = true;
        } else if (strcmp(arg, "--number-nonblank") == 0) {
            this->number_nonblank = true;
        } else if (strcmp(arg, "--show-ends") == 0) {
            this->show_ends = true;
        } else if (strcmp(arg, "--number") == 0) {
            this->number_all = true;
        } else if (strcmp(arg, "--squeeze-blank") == 0) {
            this->squeeze_blank = true;
        } else if (strcmp(arg, "--show-tabs") == 0) {
            this->show_tabs = true;
        } else if (strcmp(arg, "--show-nonprinting") == 0) {
            this->show_nonprinting = true;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            /* Single dash qualifiers or grouped flags (e.g. -A, -n, -vET) */
            for (size_t j = 1; j < strlen(arg); j++) {
                switch (arg[j]) {
                    case 'A':
                        this->show_nonprinting = true;
                        this->show_ends = true;
                        this->show_tabs = true;
                        break;
                    case 'b': this->number_nonblank = true; break;
                    case 'e':
                        this->show_nonprinting = true;
                        this->show_ends = true;
                        break;
                    case 'E': this->show_ends = true; break;
                    case 'n': this->number_all = true; break;
                    case 's': this->squeeze_blank = true; break;
                    case 't':
                        this->show_nonprinting = true;
                        this->show_tabs = true;
                        break;
                    case 'T': this->show_tabs = true; break;
                    case 'u': this->unbuffered = true; break;
                    case 'v': this->show_nonprinting = true; break;
                    default:
                        fprintf(stderr, "%s: invalid option -- '%c'\n", CAT_PROGRAM_NAME, arg[j]);
                        fprintf(stderr, "Try '%s --help' for more information.\n", CAT_PROGRAM_NAME);
                        return false;
                }
            }
        } else {
            /* File path or '-' */
            this->AddFile(this, arg);
        }
    }

    /* If no files provided, default to stdin */
    if (this->file_count == 0) {
        this->AddFile(this, "-");
    }

    /* -b overrides -n */
    if (this->number_nonblank) {
        this->number_all = false;
    }

    return true;
}

/* ============================================================================
 * CLASS: CatStream
 * Encapsulates binary file / standard input stream abstraction.
 * ============================================================================ */
typedef struct CatStream CatStream;

struct CatStream {
    /* State */
    FILE* handle;
    char* name;
    bool  is_stdin;

    /* Methods */
    bool   (*Open)(CatStream* this, const char* filename);
    int    (*ReadByte)(CatStream* this);
    size_t (*ReadBlock)(CatStream* this, unsigned char* buffer, size_t size);
    void   (*Close)(CatStream* this);
};

static bool   CatStream_Open(CatStream* this, const char* filename);
static int    CatStream_ReadByte(CatStream* this);
static size_t CatStream_ReadBlock(CatStream* this, unsigned char* buffer, size_t size);
static void   CatStream_Close(CatStream* this);

CatStream* New_CatStream(void) {
    CatStream* stream = (CatStream*)malloc(sizeof(CatStream));
    if (!stream) return NULL;

    stream->handle   = NULL;
    stream->name     = NULL;
    stream->is_stdin = false;

    /* Method binding */
    stream->Open      = CatStream_Open;
    stream->ReadByte  = CatStream_ReadByte;
    stream->ReadBlock = CatStream_ReadBlock;
    stream->Close     = CatStream_Close;

    return stream;
}

void Delete_CatStream(CatStream* this) {
    if (!this) return;
    this->Close(this);
    free(this);
}

static bool CatStream_Open(CatStream* this, const char* filename) {
    this->Close(this);
    this->name = _strdup(filename);

    if (strcmp(filename, "-") == 0) {
        this->handle = stdin;
        this->is_stdin = true;
        /* Enforce binary mode on Windows standard input */
        _setmode(_fileno(stdin), _O_BINARY);
    } else {
        /* Open in binary mode ("rb") to prevent CRLF auto-translation */
        int open_error = fopen_s(&this->handle, filename, "rb");
        if (open_error != 0) {
            char error_message[256];
            strerror_s(error_message, sizeof(error_message), open_error);
            this->handle = NULL;
            this->is_stdin = false;
            fprintf(stderr, "%s: %s: %s\n", CAT_PROGRAM_NAME, filename, error_message);
            return false;
        }
        this->is_stdin = false;
    }
    return true;
}

static int CatStream_ReadByte(CatStream* this) {
    if (!this->handle) return EOF;
    return fgetc(this->handle);
}

static size_t CatStream_ReadBlock(CatStream* this, unsigned char* buffer, size_t size) {
    if (!this->handle) return 0;
    return fread(buffer, 1, size, this->handle);
}

static void CatStream_Close(CatStream* this) {
    if (this->handle && !this->is_stdin) {
        fclose(this->handle);
    }
    if (this->name) {
        free(this->name);
        this->name = NULL;
    }
    this->handle = NULL;
    this->is_stdin = false;
}

/* ============================================================================
 * CLASS: CatEngine
 * Encapsulates the execution logic and line-processing pipeline.
 * ============================================================================ */
typedef struct CatEngine CatEngine;

struct CatEngine {
    /* State */
    const CatOptions* options;
    unsigned long     line_number;
    bool              at_line_start;
    int               consecutive_empty_lines;

    /* Methods */
    bool (*Execute)(CatEngine* this);
    bool (*ProcessFile)(CatEngine* this, CatStream* stream, const char* filename);
    void (*EmitChar)(CatEngine* this, unsigned char ch);
};

static bool CatEngine_Execute(CatEngine* this);
static bool CatEngine_ProcessFile(CatEngine* this, CatStream* stream, const char* filename);
static void CatEngine_EmitChar(CatEngine* this, unsigned char ch);

CatEngine* New_CatEngine(const CatOptions* options) {
    CatEngine* engine = (CatEngine*)malloc(sizeof(CatEngine));
    if (!engine) return NULL;

    engine->options = options;
    engine->line_number = 1;
    engine->at_line_start = true;
    engine->consecutive_empty_lines = 0;

    /* Method binding */
    engine->Execute     = CatEngine_Execute;
    engine->ProcessFile = CatEngine_ProcessFile;
    engine->EmitChar    = CatEngine_EmitChar;

    return engine;
}

void Delete_CatEngine(CatEngine* this) {
    if (this) free(this);
}

static void CatEngine_EmitChar(CatEngine* this, unsigned char ch) {
    const CatOptions* opt = this->options;

    /* Handling Tabs */
    if (ch == '\t') {
        if (opt->show_tabs) {
            fputs("^I", stdout);
        } else {
            fputc('\t', stdout);
        }
        return;
    }

    /* Handling Newlines */
    if (ch == '\n') {
        if (opt->show_ends) {
            fputc('$', stdout);
        }
        fputc('\n', stdout);
        return;
    }

    /* Handling Non-printing Characters (-v) */
    if (opt->show_nonprinting) {
        if (ch >= 128) {
            fputs("M-", stdout);
            ch -= 128;
        }
        if (ch < 32) {
            fputc('^', stdout);
            fputc(ch + 64, stdout);
            return;
        } else if (ch == 127) {
            fputc('^', stdout);
            fputc('?', stdout);
            return;
        }
    }

    fputc(ch, stdout);
}

#define CAT_READ_BUFFER_SIZE 65536

static bool CatEngine_ProcessFile(CatEngine* this, CatStream* stream, const char* filename) {
    if (!stream->Open(stream, filename)) {
        return false;
    }

    /* Fast path: no transformations — stream blocks straight through. */
    const CatOptions* opt = this->options;
    bool passthrough = !opt->number_all && !opt->number_nonblank && !opt->squeeze_blank &&
                       !opt->show_ends && !opt->show_tabs && !opt->show_nonprinting;

    unsigned char buffer[CAT_READ_BUFFER_SIZE];
    size_t bytesRead;

    if (passthrough) {
        while ((bytesRead = stream->ReadBlock(stream, buffer, sizeof(buffer))) > 0) {
            fwrite(buffer, 1, bytesRead, stdout);
        }
        return true;
    }

    /* Transforming path: block-read, then scan byte-by-byte within the block. */
    while ((bytesRead = stream->ReadBlock(stream, buffer, sizeof(buffer))) > 0) {
        for (size_t k = 0; k < bytesRead; k++) {
            unsigned char ch = buffer[k];

            /* Beginning of line checks */
            if (this->at_line_start) {
                if (ch == '\n') {
                    this->consecutive_empty_lines++;
                    /* Squeeze blank logic (-s) */
                    if (opt->squeeze_blank && this->consecutive_empty_lines > 1) {
                        continue;
                    }
                    /* Print line number on empty line only if -n is active */
                    if (opt->number_all) {
                        printf("%6lu\t", this->line_number++);
                    }
                } else {
                    this->consecutive_empty_lines = 0;
                    /* Print line number if -n or -b is active */
                    if (opt->number_all || opt->number_nonblank) {
                        printf("%6lu\t", this->line_number++);
                    }
                }
                this->at_line_start = false;
            }

            /* Emit the character */
            this->EmitChar(this, ch);

            /* Newline marks the end of the line */
            if (ch == '\n') {
                this->at_line_start = true;
            }
        }
    }

    return true;
}

static bool CatEngine_Execute(CatEngine* this) {
    /* Set stdout to binary mode to preserve line endings & exact bytes */
    _setmode(_fileno(stdout), _O_BINARY);

    if (this->options->unbuffered) {
        setvbuf(stdout, NULL, _IONBF, 0);
    }

    CatStream* stream = New_CatStream();
    bool all_succeeded = true;

    for (int i = 0; i < this->options->file_count; i++) {
        if (!this->ProcessFile(this, stream, this->options->files[i])) {
            all_succeeded = false;
        }
    }

    Delete_CatStream(stream);
    fflush(stdout);
    return all_succeeded;
}

/* ============================================================================
 * MAIN ENTRY POINT
 * ============================================================================ */
int main(int argc, char** argv) {
    CatOptions* options = New_CatOptions();
    if (!options) {
        fprintf(stderr, "%s: memory allocation failure\n", CAT_PROGRAM_NAME);
        return EXIT_FAILURE;
    }

    if (!options->Parse(options, argc, argv)) {
        Delete_CatOptions(options);
        return EXIT_FAILURE;
    }

    CatEngine* engine = New_CatEngine(options);
    if (!engine) {
        fprintf(stderr, "%s: memory allocation failure\n", CAT_PROGRAM_NAME);
        Delete_CatOptions(options);
        return EXIT_FAILURE;
    }

    bool success = engine->Execute(engine);

    Delete_CatEngine(engine);
    Delete_CatOptions(options);

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}