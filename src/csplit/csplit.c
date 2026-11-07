/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * csplit command-line entry point.
 * Argument parsing lives here; matching, execution, and output are separate modules.
 */

#define _CRT_SECURE_NO_WARNINGS
#include "csplit_engine.h"
#include "output_file_manager.h"
#include "pattern.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static void PrintUsage(void) {
    printf(
        "NAME\n"
        "    csplit - split a file into context-determined pieces\n\n"
        "SYNOPSIS\n"
        "    csplit [OPTIONS] FILE PATTERN [PATTERN...]\n\n"
        "DESCRIPTION\n"
        "    Outputs pieces of FILE separated by PATTERN(s) to files 'xx00', 'xx01', ...\n"
        "    and writes the byte counts of each piece to standard output.\n\n"
        "OPTIONS\n"
        "    -f, --prefix=PREFIX     Use PREFIX instead of 'xx'\n"
        "    -b, --suffix-format=FMT Use safe numeric suffix format (default %%02d)\n"
        "    -n, --digits=DIGITS     Use DIGITS digits instead of 2\n"
        "    -k, --keep-files        Do not remove output files on errors\n"
        "    -z, --elide-empty-files Suppress zero-length output files\n"
        "    -s, -q, --silent, --quiet\n"
        "                            Do not print counts of output file sizes\n"
        "    -h, -?, --help          Display this help message and exit\n"
        "    -V, --version           Display version information and exit\n\n"
        "PATTERNS\n"
        "    N                       Split up to, but not including, line N\n"
        "    /REGEXP/[OFFSET]        Split before a line matching REGEXP plus OFFSET\n"
        "    %%REGEXP%%[OFFSET]        Skip the matching line and split at its OFFSET\n"
        "    {N}                     Repeat previous pattern N times\n"
        "    {*}                     Repeat previous pattern as many times as possible\n\n"
    );
}

static bool ParseUnsigned(const char* text, unsigned long long* value) {
    if (!text || !*text) return false;
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
        if (!isdigit(*p)) return false;
    }
    errno = 0;
    char* end = NULL;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno == ERANGE || !end || *end != '\0') return false;
    *value = parsed;
    return true;
}

static bool ParseSignedOffset(const char* text, long long* value) {
    if (!text || (*text != '+' && *text != '-')) return false;
    if (!isdigit((unsigned char)text[1])) return false;
    errno = 0;
    char* end = NULL;
    long long parsed = strtoll(text, &end, 10);
    if (errno == ERANGE || !end || *end != '\0') return false;
    *value = parsed;
    return true;
}

static bool ParsePattern(const char* token, long long repeat_count, Pattern** result) {
    *result = NULL;
    if (token[0] == '/' || token[0] == '%') {
        char delimiter = token[0];
        const char* end = strrchr(token + 1, delimiter);
        if (!end) return false;

        long long offset = 0;
        if (end[1] && !ParseSignedOffset(end + 1, &offset)) return false;
        size_t expression_length = (size_t)(end - token - 1);
        if (expression_length == SIZE_MAX) return false;
        char* expression = (char*)malloc(expression_length + 1);
        if (!expression) return false;
        memcpy(expression, token + 1, expression_length);
        expression[expression_length] = '\0';
        *result = Pattern_create_regex(
            delimiter == '/' ? PATTERN_REGEX_SPLIT : PATTERN_REGEX_SKIP,
            expression, offset, repeat_count);
        free(expression);
        return *result != NULL;
    }

    unsigned long long line_number;
    if (!ParseUnsigned(token, &line_number) || line_number == 0 || line_number > SIZE_MAX) {
        return false;
    }
    *result = Pattern_create_line((size_t)line_number, repeat_count);
    return *result != NULL;
}

static bool ParseRepeat(const char* text, long long* repeat_count) {
    size_t length = strlen(text);
    if (length == 3 && text[0] == '{' && text[1] == '*' && text[2] == '}') {
        *repeat_count = -1;
        return true;
    }
    if (length < 3 || text[0] != '{' || text[length - 1] != '}') return false;

    char* digits = (char*)malloc(length - 1);
    if (!digits) return false;
    memcpy(digits, text + 1, length - 2);
    digits[length - 2] = '\0';
    unsigned long long parsed;
    bool valid = ParseUnsigned(digits, &parsed) && parsed <= LLONG_MAX;
    free(digits);
    if (!valid) return false;
    *repeat_count = (long long)parsed;
    return true;
}

static void DestroyPatterns(Pattern** patterns, size_t count) {
    for (size_t i = 0; i < count; ++i) Pattern_destroy(patterns[i]);
    free(patterns);
}

static const char* GetOptionValue(int argc, char* argv[], int* argument_index,
                                  const char* attached_value) {
    if (attached_value) return attached_value;
    if (*argument_index + 1 >= argc) return NULL;
    (*argument_index)++;
    return argv[*argument_index];
}

typedef enum {
    OPTION_PARSE_ERROR,
    OPTION_PARSE_OK,
    OPTION_PARSE_HELP,
    OPTION_PARSE_VERSION
} OptionParseResult;

static OptionParseResult ParseOptions(int argc, char* argv[], int* argument_index,
                                      const char** prefix, const char** suffix_format,
                                      int* digits, bool* keep_files, bool* quiet,
                                      bool* elide_empty) {
    while (*argument_index < argc) {
        const char* argument = argv[*argument_index];
        if (strcmp(argument, "/?") == 0 || strcmp(argument, "-h") == 0 ||
            strcmp(argument, "-?") == 0 || strcmp(argument, "--help") == 0) {
            PrintUsage();
            return OPTION_PARSE_HELP;
        }
        if (strcmp(argument, "--") == 0) {
            (*argument_index)++;
            break;
        }
        if (strcmp(argument, "-") == 0 || argument[0] != '-') break;
        if (strcmp(argument, "-V") == 0 || strcmp(argument, "--version") == 0) {
            puts("csplit (crossshell) 1.0");
            return OPTION_PARSE_VERSION;
        }
        if (strcmp(argument, "-k") == 0 || strcmp(argument, "--keep-files") == 0) {
            *keep_files = true;
        } else if (strcmp(argument, "-s") == 0 || strcmp(argument, "-q") == 0 ||
                   strcmp(argument, "--silent") == 0 || strcmp(argument, "--quiet") == 0) {
            *quiet = true;
        } else if (strcmp(argument, "-z") == 0 || strcmp(argument, "--elide-empty-files") == 0) {
            *elide_empty = true;
        } else {
            const char* equals = strchr(argument, '=');
            size_t option_length = equals ? (size_t)(equals - argument) : strlen(argument);
            const char* attached_value = equals ? equals + 1 : NULL;
            if (argument[0] == '-' && argument[1] != '-' &&
                (argument[1] == 'f' || argument[1] == 'n' || argument[1] == 'b')) {
                option_length = 2;
                if (argument[2] == '=') attached_value = argument + 3;
                else if (argument[2]) attached_value = argument + 2;
                else attached_value = NULL;
            }

            bool prefix_option = (option_length == 2 && strncmp(argument, "-f", 2) == 0) ||
                                 (option_length == 8 && strncmp(argument, "--prefix", 8) == 0);
            bool digits_option = (option_length == 2 && strncmp(argument, "-n", 2) == 0) ||
                                 (option_length == 8 && strncmp(argument, "--digits", 8) == 0);
            bool format_option = (option_length == 2 && strncmp(argument, "-b", 2) == 0) ||
                                 (option_length == 15 &&
                                  strncmp(argument, "--suffix-format", 15) == 0);
            if (!prefix_option && !digits_option && !format_option) {
                fprintf(stderr, "csplit: unknown option '%s'\n", argument);
                return OPTION_PARSE_ERROR;
            }

            const char* value = GetOptionValue(argc, argv, argument_index, attached_value);
            if (!value) {
                fprintf(stderr, "csplit: option '%.*s' requires a value\n",
                        (int)option_length, argument);
                return OPTION_PARSE_ERROR;
            }
            if (prefix_option) {
                *prefix = value;
            } else if (digits_option) {
                unsigned long long parsed;
                if (!ParseUnsigned(value, &parsed) || parsed == 0 || parsed > 64) {
                    fprintf(stderr, "csplit: invalid digit count '%s'\n", value);
                    return OPTION_PARSE_ERROR;
                }
                *digits = (int)parsed;
            } else {
                if (!OutputFileManager_validate_suffix_format(value)) {
                    fprintf(stderr, "csplit: invalid/unsafe suffix format '%s'\n", value);
                    return OPTION_PARSE_ERROR;
                }
                *suffix_format = value;
            }
        }
        (*argument_index)++;
    }
    return OPTION_PARSE_OK;
}

int main(int argc, char* argv[]) {
    const char* prefix = "xx";
    const char* suffix_format = NULL;
    int digits = 2;
    bool keep_files = false;
    bool quiet = false;
    bool elide_empty = false;
    int argument_index = 1;

    if (argc < 2) {
        PrintUsage();
        return EXIT_FAILURE;
    }
    OptionParseResult option_result =
        ParseOptions(argc, argv, &argument_index, &prefix, &suffix_format, &digits,
                     &keep_files, &quiet, &elide_empty);
    if (option_result != OPTION_PARSE_OK) {
        return option_result == OPTION_PARSE_HELP || option_result == OPTION_PARSE_VERSION
            ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (!OutputFileManager_validate_prefix(prefix)) {
        fprintf(stderr, "csplit: output prefix must be a plain filename prefix without path or reserved characters\n");
        return EXIT_FAILURE;
    }
    if (argument_index >= argc) {
        fprintf(stderr, "csplit: missing input file operand\n");
        return EXIT_FAILURE;
    }

    const char* input_name = argv[argument_index++];
    FILE* input = NULL;
    bool owns_input = false;
    if (strcmp(input_name, "-") == 0) {
        input = stdin;
#ifdef _WIN32
        if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
            fprintf(stderr, "csplit: cannot switch standard input to binary mode\n");
            return EXIT_FAILURE;
        }
#endif
    } else {
        input = fopen(input_name, "rb");
        if (!input) {
            fprintf(stderr, "csplit: cannot open '%s': %s\n", input_name, strerror(errno));
            return EXIT_FAILURE;
        }
        owns_input = true;
    }
    if (argument_index >= argc) {
        fprintf(stderr, "csplit: missing pattern operand\n");
        if (owns_input) fclose(input);
        return EXIT_FAILURE;
    }

    size_t pattern_capacity = 8;
    size_t pattern_count = 0;
    size_t max_lookbehind = 0;
    Pattern** patterns = (Pattern**)malloc(pattern_capacity * sizeof(Pattern*));
    if (!patterns) {
        fprintf(stderr, "csplit: out of memory parsing patterns\n");
        if (owns_input) fclose(input);
        return EXIT_FAILURE;
    }

    while (argument_index < argc) {
        const char* token = argv[argument_index++];
        long long repeat_count = 0;
        if (argument_index < argc && argv[argument_index][0] == '{') {
            if (!ParseRepeat(argv[argument_index], &repeat_count)) {
                fprintf(stderr, "csplit: invalid repeat count '%s'\n", argv[argument_index]);
                DestroyPatterns(patterns, pattern_count);
                if (owns_input) fclose(input);
                return EXIT_FAILURE;
            }
            argument_index++;
        }

        Pattern* pattern = NULL;
        if (!ParsePattern(token, repeat_count, &pattern)) {
            fprintf(stderr, "csplit: invalid pattern '%s' or insufficient memory\n", token);
            DestroyPatterns(patterns, pattern_count);
            if (owns_input) fclose(input);
            return EXIT_FAILURE;
        }
        if (pattern->offset < 0) {
            unsigned long long distance =
                (unsigned long long)(-(pattern->offset + 1)) + 1ULL;
            if (distance > SIZE_MAX) {
                fprintf(stderr, "csplit: pattern offset exceeds supported line range\n");
                Pattern_destroy(pattern);
                DestroyPatterns(patterns, pattern_count);
                if (owns_input) fclose(input);
                return EXIT_FAILURE;
            }
            if ((size_t)distance > max_lookbehind) max_lookbehind = (size_t)distance;
        }

        if (pattern_count == pattern_capacity) {
            if (pattern_capacity > SIZE_MAX / 2 ||
                pattern_capacity * 2 > SIZE_MAX / sizeof(Pattern*)) {
                fprintf(stderr, "csplit: too many patterns\n");
                Pattern_destroy(pattern);
                DestroyPatterns(patterns, pattern_count);
                if (owns_input) fclose(input);
                return EXIT_FAILURE;
            }
            size_t new_capacity = pattern_capacity * 2;
            Pattern** grown = (Pattern**)realloc(patterns,
                                                 new_capacity * sizeof(Pattern*));
            if (!grown) {
                fprintf(stderr, "csplit: out of memory parsing patterns\n");
                Pattern_destroy(pattern);
                DestroyPatterns(patterns, pattern_count);
                if (owns_input) fclose(input);
                return EXIT_FAILURE;
            }
            patterns = grown;
            pattern_capacity = new_capacity;
        }
        patterns[pattern_count++] = pattern;
    }

    OutputFileManager* output = OutputFileManager_create(
        prefix, digits, suffix_format, keep_files, quiet, elide_empty);
    if (!output) {
        fprintf(stderr, "csplit: cannot initialize output manager\n");
        DestroyPatterns(patterns, pattern_count);
        if (owns_input) fclose(input);
        return EXIT_FAILURE;
    }

    bool success = CSplitEngine_run(input, patterns, pattern_count,
                                    max_lookbehind, output);
    if (owns_input && fclose(input) != 0) {
        fprintf(stderr, "csplit: error closing input file: %s\n", strerror(errno));
        OutputFileManager_rollback(output);
        success = false;
    }
    DestroyPatterns(patterns, pattern_count);
    OutputFileManager_destroy(output);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
