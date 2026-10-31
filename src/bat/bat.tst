/**
 * bat (CrossShell for UNIX )
 * Copyright (C) 2026, PC/OpenSystems LLC
 * All Rights Reserved
 * Licensed under the terms of the BSD-3-Clause License
 * 
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _TRUNCATE
#define _TRUNCATE ((size_t)-1)
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <io.h>

#define BAT_VERSION "1.0.2 (Windows x64)"

/* ========================================================================= */
/* ANSI Terminal Escape Sequences                                            */
/* ========================================================================= */
#define ANSI_RESET        "\x1b[0m"
#define ANSI_BOLD         "\x1b[1m"
#define ANSI_DIM          "\x1b[2m"
#define ANSI_FG_GRAY      "\x1b[90m"
#define ANSI_FG_CYAN      "\x1b[36m"
#define ANSI_FG_YELLOW    "\x1b[33m"
#define ANSI_FG_GREEN     "\x1b[32m"
#define ANSI_FG_MAGENTA   "\x1b[35m"
#define ANSI_FG_BLUE      "\x1b[34m"
#define ANSI_FG_RED       "\x1b[31m"
#define ANSI_FG_WHITE     "\x1b[97m"

/* ========================================================================= */
/* CLASS: Console                                                            */
/* ========================================================================= */
typedef struct Console Console;
struct Console {
    HANDLE hOut;
    DWORD origMode;
    UINT origCP;
    bool isTty;
    bool modeSaved;
    int width;
    int height;

    /* Methods */
    void (*UpdateDimensions)(Console* self);
    void (*SetColor)(Console* self, const char* ansi);
    void (*ResetColor)(Console* self);
    void (*Print)(Console* self, const char* str);
    void (*PrintColor)(Console* self, const char* ansi, const char* str);
    void (*PrintChar)(Console* self, char ch);
};

static void Console_UpdateDimensions(Console* self) {
    if (!self->isTty) {
        self->width = 80;
        self->height = 25;
        return;
    }
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(self->hOut, &csbi)) {
        self->width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        self->height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        self->width = 80;
        self->height = 25;
    }
}

static void Console_SetColor(Console* self, const char* ansi) {
    if (self->isTty && ansi) fputs(ansi, stdout);
}

static void Console_ResetColor(Console* self) {
    if (self->isTty) fputs(ANSI_RESET, stdout);
}

static void Console_Print(Console* self, const char* str) {
    (void)self;
    if (str) fputs(str, stdout);
}

static void Console_PrintColor(Console* self, const char* ansi, const char* str) {
    if (self->isTty && ansi) fputs(ansi, stdout);
    if (str) fputs(str, stdout);
    if (self->isTty && ansi) fputs(ANSI_RESET, stdout);
}

static void Console_PrintChar(Console* self, char ch) {
    (void)self;
    putchar(ch);
}

Console* Console_New(void) {
    Console* self = (Console*)calloc(1, sizeof(Console));
    if (!self) return NULL;

    self->hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    self->isTty = (_isatty(_fileno(stdout)) != 0);
    self->origCP = GetConsoleOutputCP();
    self->modeSaved = false;

    if (self->isTty && self->hOut != INVALID_HANDLE_VALUE && self->hOut != NULL) {
        if (GetConsoleMode(self->hOut, &self->origMode)) {
            self->modeSaved = true;
            DWORD mode = self->origMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(self->hOut, mode);
        }
        SetConsoleOutputCP(CP_UTF8);
    }

    self->UpdateDimensions = Console_UpdateDimensions;
    self->SetColor = Console_SetColor;
    self->ResetColor = Console_ResetColor;
    self->Print = Console_Print;
    self->PrintColor = Console_PrintColor;
    self->PrintChar = Console_PrintChar;

    self->UpdateDimensions(self);
    return self;
}

void Console_Destroy(Console* self) {
    if (!self) return;
    if (self->isTty && self->hOut != INVALID_HANDLE_VALUE && self->hOut != NULL) {
        self->ResetColor(self);
        if (self->modeSaved) {
            SetConsoleMode(self->hOut, self->origMode);
        }
        SetConsoleOutputCP(self->origCP);
    }
    free(self);
}

/* ========================================================================= */
/* CLASS: Config                                                             */
/* ========================================================================= */
typedef struct Config Config;
struct Config {
    bool showLineNumbers;
    bool showHeader;
    bool showGrid;
    bool colorize;
    bool explicitNumbers;
    bool explicitHeader;
    bool explicitGrid;
    int tabWidth;
    int lineRangeStart;
    int lineRangeEnd;
    char languageOverride[32];
    char** files;
    int fileCount;
    int fileCapacity;

    /* Methods */
    bool (*AddFile)(Config* self, const char* path);
};

static bool Config_AddFile(Config* self, const char* path) {
    if (self->fileCount >= self->fileCapacity) {
        int newCap = (self->fileCapacity == 0) ? 16 : self->fileCapacity * 2;
        char** newFiles = (char**)realloc(self->files, sizeof(char*) * newCap);
        if (!newFiles) return false;
        self->files = newFiles;
        self->fileCapacity = newCap;
    }
    self->files[self->fileCount++] = (char*)path;
    return true;
}

Config* Config_New(void) {
    Config* self = (Config*)calloc(1, sizeof(Config));
    if (!self) return NULL;
    self->showLineNumbers = true;
    self->showHeader = true;
    self->showGrid = true;
    self->colorize = true;
    self->tabWidth = 4;
    self->lineRangeStart = 1;
    self->lineRangeEnd = 0x7FFFFFFF;
    self->fileCapacity = 16;
    self->files = (char**)malloc(sizeof(char*) * self->fileCapacity);
    self->AddFile = Config_AddFile;
    return self;
}

void Config_Destroy(Config* self) {
    if (!self) return;
    free(self->files);
    free(self);
}

/* ========================================================================= */
/* CLASS: SyntaxHighlighter                                                  */
/* ========================================================================= */
typedef struct SyntaxHighlighter SyntaxHighlighter;
struct SyntaxHighlighter {
    bool inBlockComment;
    char lang[32];

    /* Methods */
    void (*Reset)(SyntaxHighlighter* self);
    void (*SetLanguage)(SyntaxHighlighter* self, const char* lang);
    void (*DetectLanguage)(SyntaxHighlighter* self, const char* filename);
    void (*HighlightLine)(SyntaxHighlighter* self, Console* console, Config* config, const char* line);
};

static const char* C_KEYWORDS[] = {
    "auto", "bool", "break", "case", "char", "class", "const", "continue",
    "default", "do", "double", "else", "enum", "extern", "false", "float",
    "for", "goto", "if", "inline", "int", "long", "namespace", "nullptr",
    "register", "return", "short", "signed", "sizeof", "static", "struct",
    "switch", "template", "true", "typedef", "typename", "union", "unsigned",
    "using", "virtual", "void", "volatile", "while", NULL
};

static const char* PY_KEYWORDS[] = {
    "and", "as", "assert", "async", "await", "break", "class", "continue",
    "def", "del", "elif", "else", "except", "False", "finally", "for",
    "from", "global", "if", "import", "in", "is", "lambda", "None", "nonlocal",
    "not", "or", "pass", "raise", "return", "True", "try", "while", "with", "yield", NULL
};

static const char* JS_KEYWORDS[] = {
    "async", "await", "break", "case", "catch", "class", "const", "continue",
    "debugger", "default", "delete", "do", "else", "export", "extends", "false",
    "finally", "for", "function", "if", "import", "in", "instanceof", "let",
    "new", "null", "return", "super", "switch", "this", "throw", "true",
    "try", "typeof", "var", "void", "while", "with", "yield", NULL
};

static bool is_keyword_match(const char* word, const char** dict) {
    if (!dict) return false;
    for (int i = 0; dict[i] != NULL; ++i) {
        if (strcmp(word, dict[i]) == 0) return true;
    }
    return false;
}

static void Highlighter_Reset(SyntaxHighlighter* self) {
    self->inBlockComment = false;
}

static void Highlighter_SetLanguage(SyntaxHighlighter* self, const char* lang) {
    if (lang && *lang) {
        strncpy_s(self->lang, sizeof(self->lang), lang, _TRUNCATE);
    } else {
        strcpy_s(self->lang, sizeof(self->lang), "txt");
    }
}

static void Highlighter_DetectLanguage(SyntaxHighlighter* self, const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (!dot) {
        strcpy_s(self->lang, sizeof(self->lang), "txt");
        return;
    }
    if (_stricmp(dot, ".c") == 0 || _stricmp(dot, ".h") == 0 ||
        _stricmp(dot, ".cpp") == 0 || _stricmp(dot, ".hpp") == 0 ||
        _stricmp(dot, ".cc") == 0 || _stricmp(dot, ".cxx") == 0) {
        strcpy_s(self->lang, sizeof(self->lang), "c");
    } else if (_stricmp(dot, ".py") == 0) {
        strcpy_s(self->lang, sizeof(self->lang), "py");
    } else if (_stricmp(dot, ".js") == 0 || _stricmp(dot, ".ts") == 0) {
        strcpy_s(self->lang, sizeof(self->lang), "js");
    } else if (_stricmp(dot, ".json") == 0) {
        strcpy_s(self->lang, sizeof(self->lang), "json");
    } else {
        strcpy_s(self->lang, sizeof(self->lang), "txt");
    }
}

static void Highlighter_HighlightLine(SyntaxHighlighter* self, Console* console, Config* config, const char* line) {
    size_t len = strlen(line);
    size_t i = 0;
    int col = 0;

    bool isPlain = (strcmp(self->lang, "txt") == 0);
    bool isC = (strcmp(self->lang, "c") == 0);
    bool isPy = (strcmp(self->lang, "py") == 0);
    bool isJs = (strcmp(self->lang, "js") == 0);
    bool isJson = (strcmp(self->lang, "json") == 0);

    const char** dict = NULL;
    if (isC) dict = C_KEYWORDS;
    else if (isPy) dict = PY_KEYWORDS;
    else if (isJs) dict = JS_KEYWORDS;

    while (i < len) {
        /* Expand tab characters to consistent column stops (0 = pass through) */
        if (line[i] == '\t' && config->tabWidth > 0) {
            int spaces = config->tabWidth - (col % config->tabWidth);
            for (int s = 0; s < spaces; ++s) console->PrintChar(console, ' ');
            col += spaces;
            i++;
            continue;
        }

        /* If syntax highlighting or color is disabled, or plain text, pass through */
        if (!config->colorize || isPlain) {
            console->PrintChar(console, line[i++]);
            col++;
            continue;
        }

        /* Multiline block comments (only for C/C++/JS) */
        if ((isC || isJs) && self->inBlockComment) {
            console->SetColor(console, ANSI_FG_GRAY);
            while (i < len) {
                console->PrintChar(console, line[i]);
                col++;
                if (line[i] == '*' && i + 1 < len && line[i + 1] == '/') {
                    console->PrintChar(console, line[++i]);
                    col++;
                    self->inBlockComment = false;
                    console->ResetColor(console);
                    i++;
                    break;
                }
                i++;
            }
            continue;
        }

        /* Single-line comments (// for C/JS, # for Python) */
        if (((isC || isJs) && line[i] == '/' && i + 1 < len && line[i + 1] == '/') ||
            (isPy && line[i] == '#')) {
            console->SetColor(console, ANSI_FG_GRAY);
            while (i < len && line[i] != '\r' && line[i] != '\n') {
                console->PrintChar(console, line[i++]);
                col++;
            }
            console->ResetColor(console);
            while (i < len) {
                console->PrintChar(console, line[i++]);
            }
            break;
        }

        /* Block comment open (slash-star for C/JS only) */
        if ((isC || isJs) && line[i] == '/' && i + 1 < len && line[i + 1] == '*') {
            self->inBlockComment = true;
            console->SetColor(console, ANSI_FG_GRAY);
            console->PrintChar(console, line[i++]);
            console->PrintChar(console, line[i++]);
            col += 2;
            continue;
        }

        /* Preprocessor directives (C only) */
        if (isC && line[i] == '#') {
            console->SetColor(console, ANSI_FG_MAGENTA);
            while (i < len && !isspace((unsigned char)line[i])) {
                console->PrintChar(console, line[i++]);
                col++;
            }
            console->ResetColor(console);
            continue;
        }

        /* Strings and character literals */
        if (line[i] == '"' || ((isC || isPy || isJs) && line[i] == '\'')) {
            char quote = line[i];
            console->SetColor(console, ANSI_FG_GREEN);
            console->PrintChar(console, line[i++]);
            col++;
            while (i < len && line[i] != '\r' && line[i] != '\n') {
                console->PrintChar(console, line[i]);
                col++;
                if (line[i] == '\\' && i + 1 < len) {
                    console->PrintChar(console, line[++i]);
                    col++;
                } else if (line[i] == quote) {
                    i++;
                    break;
                }
                i++;
            }
            console->ResetColor(console);
            continue;
        }

        /* Numbers (leading '-' only counts when a digit follows) */
        if (isdigit((unsigned char)line[i]) ||
            (line[i] == '-' && i + 1 < len && isdigit((unsigned char)line[i + 1])) ||
            (line[i] == '.' && i + 1 < len && isdigit((unsigned char)line[i + 1]))) {
            console->SetColor(console, ANSI_FG_YELLOW);
            while (i < len && (isalnum((unsigned char)line[i]) || line[i] == '.' || line[i] == 'x' || line[i] == 'X')) {
                console->PrintChar(console, line[i++]);
                col++;
            }
            console->ResetColor(console);
            continue;
        }

        /* Word identifiers & keywords */
        if (isalpha((unsigned char)line[i]) || line[i] == '_') {
            char token[128];
            int tLen = 0;
            while (i < len && (isalnum((unsigned char)line[i]) || line[i] == '_')) {
                if (tLen < (int)sizeof(token) - 1) {
                    token[tLen++] = line[i];
                }
                i++;
            }
            token[tLen] = '\0';
            col += (int)strlen(token);

            if (dict && is_keyword_match(token, dict)) {
                console->PrintColor(console, ANSI_FG_CYAN ANSI_BOLD, token);
            } else {
                console->Print(console, token);
            }
            continue;
        }

        /* Fallback: standard characters, whitespace, punctuation */
        console->PrintChar(console, line[i++]);
        col++;
    }
}

SyntaxHighlighter* SyntaxHighlighter_New(void) {
    SyntaxHighlighter* self = (SyntaxHighlighter*)calloc(1, sizeof(SyntaxHighlighter));
    if (!self) return NULL;
    self->Reset = Highlighter_Reset;
    self->SetLanguage = Highlighter_SetLanguage;
    self->DetectLanguage = Highlighter_DetectLanguage;
    self->HighlightLine = Highlighter_HighlightLine;
    return self;
}

void SyntaxHighlighter_Destroy(SyntaxHighlighter* self) {
    free(self);
}

/* ========================================================================= */
/* CLASS: FilePrinter                                                        */
/* ========================================================================= */
typedef struct FilePrinter FilePrinter;
struct FilePrinter {
    Console* console;
    Config* config;
    SyntaxHighlighter* highlighter;

    /* Methods */
    void (*PrintHeader)(FilePrinter* self, const char* fileName);
    void (*PrintFooter)(FilePrinter* self);
    bool (*PrintFile)(FilePrinter* self, const char* fileName);
};

static void FilePrinter_PrintHeader(FilePrinter* self, const char* fileName) {
    if (!self->config->showHeader) return;

    int termWidth = self->console->width >= 40 ? self->console->width : 80;
    int prefixLen = 10; /* "───────┬─ " = 10 columns */
    int maxLabel = termWidth - prefixLen - 6; /* 6 = "File: " (6) */

    char displayPath[256];
    if ((int)strlen(fileName) > maxLabel && maxLabel > 5) {
        /* Truncate leading path components: .../filename */
        sprintf_s(displayPath, sizeof(displayPath), "...%s", fileName + (strlen(fileName) - maxLabel + 3));
    } else {
        strncpy_s(displayPath, sizeof(displayPath), fileName, _TRUNCATE);
    }

    /* Count display columns, not bytes: UTF-8 continuation bytes are zero-width */
    int displayLen = 0;
    for (const char* p = displayPath; *p; ++p) {
        if (((unsigned char)*p & 0xC0) != 0x80) displayLen++;
    }

    int fileLabelLen = 6 + displayLen + 1; /* "File: <path> " */
    int remain = termWidth - (prefixLen + fileLabelLen);
    if (remain < 0) remain = 0;

    self->console->PrintColor(self->console, ANSI_FG_GRAY, "───────┬─ ");
    self->console->PrintColor(self->console, ANSI_FG_WHITE ANSI_BOLD, "File: ");
    self->console->PrintColor(self->console, ANSI_FG_CYAN ANSI_BOLD, displayPath);
    self->console->Print(self->console, " ");
    self->console->SetColor(self->console, ANSI_FG_GRAY);
    for (int i = 0; i < remain; ++i) fputs("─", stdout);
    self->console->ResetColor(self->console);
    putchar('\n');
}

static void FilePrinter_PrintFooter(FilePrinter* self) {
    /* If both header and grid are disabled, omit the bottom border */
    if (!self->config->showGrid || !self->config->showHeader) return;

    int termWidth = self->console->width >= 40 ? self->console->width : 80;
    int remain = termWidth - 9; /* "───────┴─" = 9 columns */
    if (remain < 0) remain = 0;

    self->console->SetColor(self->console, ANSI_FG_GRAY);
    fputs("───────┴─", stdout);
    for (int i = 0; i < remain; ++i) fputs("─", stdout);
    self->console->ResetColor(self->console);
    putchar('\n');
}

static bool IsBinaryStream(FILE* fp) {
    char checkBuf[1024];
    long pos = ftell(fp);
    size_t n = fread(checkBuf, 1, sizeof(checkBuf), fp);
    if (pos >= 0) fseek(fp, pos, SEEK_SET);

    for (size_t i = 0; i < n; ++i) {
        if (checkBuf[i] == '\0') return true;
    }
    return false;
}

static bool FilePrinter_PrintFile(FilePrinter* self, const char* fileName) {
    FILE* fp = NULL;
    bool isStdin = (strcmp(fileName, "-") == 0);

    if (isStdin) {
        fp = stdin;
        fileName = "STDIN";
    } else {
        fopen_s(&fp, fileName, "r");
        if (!fp) {
            fprintf(stderr, ANSI_FG_RED "bat error: " ANSI_RESET "Could not open file '%s'\n", fileName);
            return false;
        }
        if (IsBinaryStream(fp)) {
            fprintf(stderr, ANSI_FG_YELLOW "bat: " ANSI_RESET "Binary content not shown for '%s'\n", fileName);
            fclose(fp);
            return true;
        }
    }

    self->highlighter->Reset(self->highlighter);
    if (self->config->languageOverride[0] != '\0') {
        self->highlighter->SetLanguage(self->highlighter, self->config->languageOverride);
    } else {
        self->highlighter->DetectLanguage(self->highlighter, fileName);
    }

    self->PrintHeader(self, fileName);

    char buffer[4096];
    int lineNum = 1;
    bool isLineStart = true;
    char lastChar = '\n';
    bool bomChecked = false;

    while (fgets(buffer, sizeof(buffer), fp)) {
        size_t bLen = strlen(buffer);
        if (bLen == 0) continue;

        /* Skip a UTF-8 BOM at the very start of the stream */
        if (!bomChecked) {
            bomChecked = true;
            if (bLen >= 3 && (unsigned char)buffer[0] == 0xEF &&
                (unsigned char)buffer[1] == 0xBB && (unsigned char)buffer[2] == 0xBF) {
                memmove(buffer, buffer + 3, bLen - 2); /* keeps the NUL */
                bLen -= 3;
                if (bLen == 0) continue;
            }
        }

        if (lineNum >= self->config->lineRangeStart && lineNum <= self->config->lineRangeEnd) {
            if (isLineStart && self->config->showLineNumbers) {
                char gutter[32];
                const char* pipeChar = self->config->showGrid ? "│" : " ";
                sprintf_s(gutter, sizeof(gutter), "%6d %s ", lineNum, pipeChar);
                self->console->PrintColor(self->console, ANSI_FG_GRAY, gutter);
            } else if (!isLineStart && self->config->showLineNumbers) {
                const char* pipeChar = self->config->showGrid ? "│" : " ";
                char gutter[32];
                sprintf_s(gutter, sizeof(gutter), "       %s ", pipeChar);
                self->console->PrintColor(self->console, ANSI_FG_GRAY, gutter);
            }

            self->highlighter->HighlightLine(self->highlighter, self->console, self->config, buffer);
        }

        lastChar = buffer[bLen - 1];
        if (lastChar == '\n') {
            lineNum++;
            isLineStart = true;
        } else {
            isLineStart = false;
        }
    }

    if (lastChar != '\n') {
        putchar('\n');
    }

    self->PrintFooter(self);

    if (!isStdin) {
        fclose(fp);
    }
    return true;
}

FilePrinter* FilePrinter_New(Console* console, Config* config) {
    FilePrinter* self = (FilePrinter*)calloc(1, sizeof(FilePrinter));
    if (!self) return NULL;
    self->console = console;
    self->config = config;
    self->highlighter = SyntaxHighlighter_New();
    self->PrintHeader = FilePrinter_PrintHeader;
    self->PrintFooter = FilePrinter_PrintFooter;
    self->PrintFile = FilePrinter_PrintFile;
    return self;
}

void FilePrinter_Destroy(FilePrinter* self) {
    if (!self) return;
    SyntaxHighlighter_Destroy(self->highlighter);
    free(self);
}

/* ========================================================================= */
/* CLASS: Application                                                        */
/* ========================================================================= */
typedef struct Application Application;
struct Application {
    Console* console;
    Config* config;
    FilePrinter* printer;
    int exitStatus;

    /* Methods */
    void (*PrintHelp)(Application* self);
    void (*PrintVersion)(Application* self);
    bool (*ParseArguments)(Application* self, int argc, char** argv);
    int  (*Run)(Application* self, int argc, char** argv);
};

static void Application_PrintVersion(Application* self) {
    (void)self;
    puts("bat " BAT_VERSION);
    puts("A cat(1) clone with syntax highlighting and file decorations for Windows.");
}

static void Application_PrintHelp(Application* self) {
    (void)self;
    puts(ANSI_BOLD "bat" ANSI_RESET " " BAT_VERSION);
    puts("A cat(1) clone with syntax highlighting and file decorations for Windows.\n");

    puts(ANSI_BOLD "USAGE:" ANSI_RESET);
    puts("    bat [OPTIONS] [FILE]...\n");

    puts(ANSI_BOLD "DESCRIPTION:" ANSI_RESET);
    puts("    bat prints file contents with syntax highlighting and Git-style decorations.");
    puts("    When output is redirected or piped to a file, bat automatically operates in");
    puts("    plain pass-through mode unless explicit flags are provided.\n");

    puts(ANSI_BOLD "ARGS:" ANSI_RESET);
    puts("    <FILE>...                File(s) to concatenate. Use '-' or omit for STDIN.\n");

    puts(ANSI_BOLD "OPTIONS:" ANSI_RESET);
    puts("    -n, --number             Show line numbers.");
    puts("    -p, --plain              Plain mode: disable line numbers, grid, headers, and colors.");
    puts("    -r, --line-range <N:M>   Print only lines N through M (e.g., 10:25, 20:, :50).");
    puts("    -l, --language <lang>    Force syntax language (c, py, js, json, txt).");
    puts("        --tabs <T>           Set tab stop spacing (default: 4, 0 to disable expansion).");
    puts("        --force-color        Force color output even if piped or redirected.");
    puts("        --no-color           Disable all ANSI syntax highlighting and colors.");
    puts("        --no-header          Suppress the top file metadata banner.");
    puts("        --no-grid            Suppress the vertical gutter and bottom border.");
    puts("    -h, --help               Display this help manual.");
    puts("    -V, --version            Display version information.\n");

    puts(ANSI_BOLD "EXAMPLES:" ANSI_RESET);
    puts("    bat main.c                 Display file with syntax highlighting & header");
    puts("    bat -n script.py           Show line numbers");
    puts("    bat -r 10:30 server.c      Inspect lines 10 through 30");
    puts("    bat -r 50: log.txt         Print from line 50 to the end of file");
    puts("    bat -p data.json           Display raw text without decorations");
    puts("    type build.ninja | bat     Highlight piped standard input");
}

static void ParseLineRange(const char* str, int* outStart, int* outEnd) {
    *outStart = 1;
    *outEnd = 0x7FFFFFFF;
    const char* colon = strchr(str, ':');
    if (colon) {
        if (colon != str) {
            *outStart = atoi(str);
            if (*outStart < 1) *outStart = 1;
        }
        if (*(colon + 1) != '\0') {
            *outEnd = atoi(colon + 1);
        }
    } else {
        int val = atoi(str);
        if (val < 1) val = 1;
        *outStart = val;
        *outEnd = val;
    }
}

static bool Application_ParseArguments(Application* self, int argc, char** argv) {
    bool forceColor = false;
    bool stopOptions = false;
    self->exitStatus = 0;

    for (int i = 1; i < argc; ++i) {
        if (!stopOptions && strcmp(argv[i], "--") == 0) {
            stopOptions = true;
            continue;
        }

        if (!stopOptions && argv[i][0] == '-' && argv[i][1] != '\0') {
            if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                self->PrintHelp(self);
                self->exitStatus = 0;
                return false;
            } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
                self->PrintVersion(self);
                self->exitStatus = 0;
                return false;
            } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--number") == 0) {
                self->config->showLineNumbers = true;
                self->config->explicitNumbers = true;
            } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--plain") == 0) {
                self->config->showLineNumbers = false;
                self->config->showHeader = false;
                self->config->showGrid = false;
                self->config->colorize = false;
            } else if (strcmp(argv[i], "--force-color") == 0) {
                forceColor = true;
            } else if (strcmp(argv[i], "--no-color") == 0) {
                self->config->colorize = false;
            } else if (strcmp(argv[i], "--no-header") == 0) {
                self->config->showHeader = false;
                self->config->explicitHeader = true;
            } else if (strcmp(argv[i], "--no-grid") == 0) {
                self->config->showGrid = false;
                self->config->explicitGrid = true;
            } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--language") == 0) {
                if (i + 1 < argc) {
                    strncpy_s(self->config->languageOverride, sizeof(self->config->languageOverride), argv[++i], _TRUNCATE);
                } else {
                    fprintf(stderr, ANSI_FG_RED "bat error: " ANSI_RESET "Option '%s' requires an argument.\n", argv[i]);
                    self->exitStatus = 1;
                    return false;
                }
            } else if (strcmp(argv[i], "--tabs") == 0) {
                if (i + 1 < argc) {
                    self->config->tabWidth = atoi(argv[++i]);
                    if (self->config->tabWidth < 0) self->config->tabWidth = 0; /* 0 = disable expansion */
                } else {
                    fprintf(stderr, ANSI_FG_RED "bat error: " ANSI_RESET "Option '--tabs' requires a number.\n");
                    self->exitStatus = 1;
                    return false;
                }
            } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--line-range") == 0) {
                if (i + 1 < argc) {
                    ParseLineRange(argv[++i], &self->config->lineRangeStart, &self->config->lineRangeEnd);
                } else {
                    fprintf(stderr, ANSI_FG_RED "bat error: " ANSI_RESET "Option '%s' requires a range argument (e.g. 10:20).\n", argv[i]);
                    self->exitStatus = 1;
                    return false;
                }
            } else {
                fprintf(stderr, ANSI_FG_RED "bat error: " ANSI_RESET "Unknown flag '%s'. Run 'bat --help' for manual.\n", argv[i]);
                self->exitStatus = 1;
                return false;
            }
        } else {
            self->config->AddFile(self->config, argv[i]);
        }
    }

    /* Auto-detect non-TTY redirection (pipe to file, clip, etc.) */
    if (!self->console->isTty) {
        if (!forceColor) self->config->colorize = false;
        if (!self->config->explicitHeader) self->config->showHeader = false;
        if (!self->config->explicitGrid) self->config->showGrid = false;
        if (!self->config->explicitNumbers) self->config->showLineNumbers = false;
    }

    if (self->config->fileCount == 0) {
        self->config->AddFile(self->config, "-");
    }

    return true;
}

static int Application_Run(Application* self, int argc, char** argv) {
    if (!self->ParseArguments(self, argc, argv)) {
        return self->exitStatus;
    }

    int exitCode = 0;
    for (int i = 0; i < self->config->fileCount; ++i) {
        if (!self->printer->PrintFile(self->printer, self->config->files[i])) {
            exitCode = 1;
        }
    }
    return exitCode;
}

Application* Application_New(void) {
    Application* self = (Application*)calloc(1, sizeof(Application));
    if (!self) return NULL;

    self->console = Console_New();
    self->config = Config_New();
    if (!self->console || !self->config) {
        Console_Destroy(self->console);
        Config_Destroy(self->config);
        free(self);
        return NULL;
    }

    self->printer = FilePrinter_New(self->console, self->config);
    if (!self->printer) {
        Console_Destroy(self->console);
        Config_Destroy(self->config);
        free(self);
        return NULL;
    }

    self->PrintHelp = Application_PrintHelp;
    self->PrintVersion = Application_PrintVersion;
    self->ParseArguments = Application_ParseArguments;
    self->Run = Application_Run;
    return self;
}

void Application_Destroy(Application* self) {
    if (!self) return;
    FilePrinter_Destroy(self->printer);
    Config_Destroy(self->config);
    Console_Destroy(self->console);
    free(self);
}

/* ========================================================================= */
/* Entry Point                                                               */
/* ========================================================================= */
int main(int argc, char** argv) {
    Application* app = Application_New();
    if (!app) {
        fprintf(stderr, "Fatal error: Out of memory initializing bat.\n");
        return 1;
    }
    int exitCode = app->Run(app, argc, argv);
    Application_Destroy(app);
    return exitCode;
}
