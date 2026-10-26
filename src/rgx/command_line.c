#include "command_line.h"
#include "allocation.h"
#include "options.h"
#include "unicode.h"
#include "search_state.h"
#include "text_reader.h"
#include "path_conversion.h"
#include "pattern_set.h"
#include "directory_walker.h"
#include "help.h"
#include "file_types.h"

static bool contains_uppercase(const char* s) {
    size_t remaining = strlen(s);
    while (remaining) {
        size_t width;
        uint32_t rune = utf8_rune(s, remaining, &width);
        if (rune < 128 ? isupper((unsigned char)rune) != 0 : (rune_type(rune, CT_CTYPE1) & C1_UPPER) != 0) return true;
        s += width;
        remaining -= width;
    }
    return false;
}

static bool parse_size_option(const char* option, const char* text, size_t maximum, size_t* value) {
    if (!*text) {
        fprintf(stderr, "rgx: %s requires a non-negative integer\n", option);
        return false;
    }
    size_t parsed = 0;
    for (const unsigned char* digit = (const unsigned char*)text; *digit; ++digit) {
        if (*digit < '0' || *digit > '9' || parsed > maximum / 10 ||
            (parsed == maximum / 10 && (size_t)(*digit - '0') > maximum % 10)) {
            fprintf(stderr, "rgx: invalid or excessive value for %s: %s\n", option, text);
            return false;
        }
        parsed = parsed * 10 + (size_t)(*digit - '0');
    }
    *value = parsed;
    return true;
}

static void append_option_string(RgxOptions* opts, char*** strings, size_t* count, const char* value) {
    size_t length = strlen(value);
    size_t remaining = 16U * 1024U * 1024U - opts->option_string_bytes;
    if (*count >= 65536 || remaining <= sizeof(char*) || length >= remaining - sizeof(char*)) {
        fprintf(stderr, "rgx: option list exceeds safety limit\n");
        exit(2);
    }
    *strings = (char**)rgx_realloc_array(*strings, *count + 1, sizeof(char*));
    (*strings)[(*count)++] = rgx_strdup(value);
    opts->option_string_bytes += length + 1 + sizeof(char*);
}

static bool load_pattern_file(RgxOptions* opts, const char* path) {
    FILE* stream = stdin;
    if (strcmp(path, "-") != 0) {
        wchar_t* wide = path_to_wide(path);
        if (!wide) return false;
        int error = _wfopen_s(&stream, wide, L"rb");
        free(wide);
        if (error || !stream) {
            fprintf(stderr, "rgx: cannot read pattern file: %s\n", path);
            return false;
        }
    }
    TextReader reader;
    reader_init(&reader, stream);
    char* line = NULL;
    size_t capacity = 0, length;
    bool valid = true;
    while (reader_line(&reader, &line, &capacity, &length)) {
        if (length && line[length - 1] == '\r') --length;
        if (length > 65536 || (length && memchr(line, 0, length))) {
            fprintf(stderr, "rgx: invalid or excessive pattern in %s\n", path);
            valid = false;
            break;
        }
        line = (char*)rgx_realloc_array(line, length + 1, 1);
        capacity = length + 1;
        line[length] = 0;
        append_option_string(opts, &opts->patterns, &opts->pattern_count, line);
    }
    if (ferror(stream) || rgx_had_error) valid = false;
    if (stream != stdin) fclose(stream);
    free(line);
    opts->pattern_source_set = true;
    return valid;
}

static bool set_cli_option(RgxOptions* opts, const char* option, const char* value) {
    if(strcmp(option,"--type")==0)append_option_string(opts,&opts->types,&opts->type_count,value);
    else if(strcmp(option,"--type-not")==0)append_option_string(opts,&opts->type_not,&opts->type_not_count,value);
    else if(strcmp(option,"--type-add")==0)append_option_string(opts,&opts->type_add,&opts->type_add_count,value);
    else if(strcmp(option,"--type-clear")==0)append_option_string(opts,&opts->type_clear,&opts->type_clear_count,value);
    else if(strcmp(option,"--threads")==0)return parse_size_option(option,value,16,&opts->threads);
    else if(strcmp(option,"--replace")==0){
        for(const char* cursor=value;*cursor;++cursor)if(*cursor=='$' && cursor[1] && cursor[1]!='0' && cursor[1]!='$'){
            fprintf(stderr,"rgx: replacement supports $0 and $$, not capture references\n");return false;
        }
        free(opts->replacement);opts->replacement=rgx_strdup(value);
    }
    else if(strcmp(option,"--json")==0){opts->json=true;opts->color_mode=COLOR_NEVER;}
    else if(strcmp(option,"--byte-offset")==0)opts->byte_offset=true;
    else if(strcmp(option,"--column")==0)opts->column=true;
    else if(strcmp(option,"--null")==0)opts->null_paths=true;
    else if(strcmp(option,"--heading")==0){opts->heading=true;if(opts->filename_mode<0)opts->filename_mode=1;}
    else if(strcmp(option,"--no-heading")==0)opts->heading=false;
    else if(strcmp(option,"--stats")==0)opts->stats=true;
    else if(strcmp(option,"--multiline")==0)opts->multiline=true;
    else if(strcmp(option,"--multiline-dotall")==0){opts->multiline=true;opts->multiline_dotall=true;}
    else if(strcmp(option,"--no-require-git")==0)opts->no_require_git=true;
    else if(strcmp(option,"--type-list")==0)opts->type_list=true;
    else if (strcmp(option, "--regexp") == 0) {
        append_option_string(opts, &opts->patterns, &opts->pattern_count, value);
        opts->pattern_source_set = true;
    } else if (strcmp(option, "--file") == 0) {
        return load_pattern_file(opts, value);
    } else if (strcmp(option, "--glob") == 0) {
        append_option_string(opts, &opts->globs, &opts->glob_count, value);
    } else if (strcmp(option, "--color") == 0) {
        if (strcmp(value, "always") == 0) opts->color_mode = COLOR_ALWAYS;
        else if (strcmp(value, "never") == 0) opts->color_mode = COLOR_NEVER;
        else if (strcmp(value, "auto") == 0) opts->color_mode = COLOR_AUTO;
        else { fprintf(stderr, "rgx: invalid color mode: %s\n", value); return false; }
    } else if (strcmp(option, "--max-count") == 0) {
        if (!parse_size_option(option, value, SIZE_MAX, &opts->max_count)) return false;
        opts->max_count_set = true;
    } else if (strcmp(option, "--max-depth") == 0) {
        if (!parse_size_option(option, value, 512, &opts->max_depth)) return false;
        opts->max_depth_set = true;
    } else if (strcmp(option, "--before-context") == 0) {
        return parse_size_option(option, value, 1048576, &opts->before_context);
    } else if (strcmp(option, "--after-context") == 0) {
        return parse_size_option(option, value, 1048576, &opts->after_context);
    } else if (strcmp(option, "--context") == 0) {
        if (!parse_size_option(option, value, 1048576, &opts->before_context)) return false;
        opts->after_context = opts->before_context;
    } else if (strcmp(option, "--ignore-case") == 0) {
        opts->ignore_case = true; opts->smart_case = opts->case_sensitive = false;
    } else if (strcmp(option, "--smart-case") == 0) {
        opts->smart_case = true; opts->ignore_case = opts->case_sensitive = false;
    } else if (strcmp(option, "--case-sensitive") == 0) {
        opts->case_sensitive = true; opts->ignore_case = opts->smart_case = false;
    } else if (strcmp(option, "--fixed-strings") == 0) opts->fixed_strings = true;
    else if (strcmp(option, "--invert-match") == 0) opts->invert_match = true;
    else if (strcmp(option, "--word-regexp") == 0) opts->word_regexp = true;
    else if (strcmp(option, "--line-regexp") == 0) opts->line_regexp = true;
    else if (strcmp(option, "--line-number") == 0) opts->show_line_numbers = true;
    else if (strcmp(option, "--no-line-number") == 0) opts->show_line_numbers = false;
    else if (strcmp(option, "--with-filename") == 0) opts->filename_mode = 1;
    else if (strcmp(option, "--no-filename") == 0) opts->filename_mode = 0;
    else if (strcmp(option, "--count") == 0) { opts->count_only = true; opts->count_matches = false; }
    else if (strcmp(option, "--count-matches") == 0) { opts->count_only = true; opts->count_matches = true; }
    else if (strcmp(option, "--files-with-matches") == 0) { opts->files_with_matches = true; opts->files_without_match = false; }
    else if (strcmp(option, "--files-without-match") == 0) { opts->files_without_match = true; opts->files_with_matches = false; }
    else if (strcmp(option, "--quiet") == 0) opts->quiet = true;
    else if (strcmp(option, "--only-matching") == 0) opts->only_matching = true;
    else if (strcmp(option, "--hidden") == 0) opts->search_hidden = true;
    else if (strcmp(option, "--no-ignore") == 0) opts->no_ignore = true;
    else if (strcmp(option, "--text") == 0) opts->text = true;
    else if (strcmp(option, "--crlf") == 0) opts->crlf = true;
    else if (strcmp(option, "--files") == 0) opts->files_only = true;
    else { fprintf(stderr, "rgx: unrecognized option: %s\n", option); return false; }
    return true;
}

static bool option_requires_value(const char* option) {
    return strcmp(option,"--type")==0 || strcmp(option,"--type-not")==0 || strcmp(option,"--type-add")==0 ||
        strcmp(option,"--type-clear")==0 || strcmp(option,"--replace")==0 || strcmp(option,"--threads")==0 ||
        strcmp(option, "--regexp") == 0 || strcmp(option, "--file") == 0 ||
        strcmp(option, "--glob") == 0 || strcmp(option, "--color") == 0 ||
        strcmp(option, "--max-count") == 0 || strcmp(option, "--max-depth") == 0 ||
        strcmp(option, "--before-context") == 0 || strcmp(option, "--after-context") == 0 || strcmp(option, "--context") == 0;
}

int rgx_main(int argc, char** argv) {
    if (argc < 2) {
        print_help();
        return 2;
    }

    RgxOptions* opts = RgxOptions_New();
    bool positional_only = false;

    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];
        if (!positional_only && strcmp(arg, "--") == 0) { positional_only = true; continue; }
        if (!positional_only && (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)) {
            print_help();
            RgxOptions_Free(opts);
            return 0;
        } else if (!positional_only && (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0)) {
            printf("rgx %s\n", RGX_VERSION);
            RgxOptions_Free(opts);
            return 0;
        } else if (!positional_only && arg[0] == '-' && arg[1]) {
            if (arg[1] == '-') {
                char* value = strchr(arg + 2, '=');
                if (value) *value++ = 0;
                bool required = option_requires_value(arg);
                if (required && !value) {
                    if (i + 1 == argc) { fprintf(stderr, "rgx: option requires a value: %s\n", arg); goto invalid_options; }
                    value = argv[++i];
                }
                if ((!required && value) || !set_cli_option(opts, arg, value)) goto invalid_options;
            } else {
                for (size_t option_index = 1; arg[option_index]; ++option_index) {
                    const char* option = NULL;
                    switch (arg[option_index]) {
                        case 'F': option = "--fixed-strings"; break;
                        case 't': option = "--type"; break;
                        case 'T': option = "--type-not"; break;
                        case 'j': option = "--threads"; break;
                        case 'r': option = "--replace"; break;
                        case 'b': option = "--byte-offset"; break;
                        case '0': option = "--null"; break;
                        case 'U': option = "--multiline"; break;
                        case 'i': option = "--ignore-case"; break;
                        case 's': option = "--case-sensitive"; break;
                        case 'S': option = "--smart-case"; break;
                        case 'v': option = "--invert-match"; break;
                        case 'w': option = "--word-regexp"; break;
                        case 'x': option = "--line-regexp"; break;
                        case 'n': option = "--line-number"; break;
                        case 'N': option = "--no-line-number"; break;
                        case 'H': option = "--with-filename"; break;
                        case 'I': option = "--no-filename"; break;
                        case 'c': option = "--count"; break;
                        case 'l': option = "--files-with-matches"; break;
                        case 'o': option = "--only-matching"; break;
                        case 'q': option = "--quiet"; break;
                        case 'a': option = "--text"; break;
                        case 'e': option = "--regexp"; break;
                        case 'f': option = "--file"; break;
                        case 'g': option = "--glob"; break;
                        case 'm': option = "--max-count"; break;
                        case 'A': option = "--after-context"; break;
                        case 'B': option = "--before-context"; break;
                        case 'C': option = "--context"; break;
                        default: fprintf(stderr, "rgx: unrecognized short option: -%c\n", arg[option_index]); goto invalid_options;
                    }
                    const char* value = NULL;
                    bool required = option_requires_value(option);
                    if (required) {
                        value = arg + option_index + 1;
                        if (!*value) {
                            if (i + 1 == argc) { fprintf(stderr, "rgx: option requires a value: %s\n", option); goto invalid_options; }
                            value = argv[++i];
                        }
                    }
                    if (!set_cli_option(opts, option, value)) goto invalid_options;
                    if (required) break;
                }
            }
        } else {
            if (!opts->pattern_count && !opts->pattern_source_set && !opts->files_only) {
                append_option_string(opts, &opts->patterns, &opts->pattern_count, arg);
            } else {
                append_option_string(opts, &opts->paths, &opts->path_count, arg);
            }
        }
    }

    if(!file_types_validate(opts))goto invalid_options;
    if(opts->multiline && opts->invert_match){fprintf(stderr,"rgx: inverted multiline search is unsupported\n");goto invalid_options;}
    if(opts->multiline && opts->replacement && !opts->only_matching){fprintf(stderr,"rgx: multiline replacement requires --only-matching\n");goto invalid_options;}
    if(opts->type_list){file_types_list(opts);RgxOptions_Free(opts);return 0;}
    if(opts->json && (opts->count_only || opts->files_only || opts->files_with_matches || opts->files_without_match)) {
        fprintf(stderr,"rgx: JSON events cannot be combined with count/file-list modes\n");goto invalid_options;
    }
    if (!opts->pattern_count && !opts->pattern_source_set && !opts->files_only) {
        fprintf(stderr, "rgx: error: no pattern provided. Run with --help for details.\n");
        RgxOptions_Free(opts);
        return 2;
    }

    /* Fallback to current directory '.' if no paths were provided */
    if (opts->path_count == 0) {
        DWORD input_type = GetFileType(GetStdHandle(STD_INPUT_HANDLE));
        append_option_string(opts, &opts->paths, &opts->path_count,
            !opts->files_only && (input_type == FILE_TYPE_PIPE || input_type == FILE_TYPE_DISK) ? "-" : ".");
    }

    /* Resolve case-sensitivity mode */
    bool effective_ignore_case = opts->ignore_case;
    if (opts->smart_case) {
        effective_ignore_case = true;
        for (size_t index = 0; index < opts->pattern_count; ++index) {
            if (contains_uppercase(opts->patterns[index])) effective_ignore_case = false;
        }
    }
    if (opts->case_sensitive) {
        effective_ignore_case = false;
    }
    opts->effective_ignore_case=effective_ignore_case;

    /* Instantiate polymorphically based on engine qualifiers */
    IMatcher* matcher = PatternSet_New(opts, effective_ignore_case);

    if (!matcher) {
        RgxOptions_Free(opts);
        return 2;
    }

    RgxPrinter* printer = RgxPrinter_New(opts);
    bool multiple_targets = opts->path_count > 1;
    for (size_t index = 0; index < opts->path_count && !multiple_targets; ++index) {
        if (strcmp(opts->paths[index], "-") == 0) continue;
        wchar_t* wide = path_to_wide(opts->paths[index]);
        if (!wide) continue;
        DWORD attributes = GetFileAttributesW(wide);
        free(wide);
        multiple_targets = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    RgxWalker* walker = RgxWalker_New(opts, matcher, printer, multiple_targets);
    bool has_stdin=false;
    for(size_t index=0;index<opts->path_count;++index)if(strcmp(opts->paths[index],"-")==0)has_stdin=true;
    size_t workers=opts->threads;
    if(!workers){SYSTEM_INFO system;GetSystemInfo(&system);workers=system.dwNumberOfProcessors;if(workers>4)workers=4;}
    if(workers>1 && multiple_targets && !has_stdin && !opts->quiet && !opts->files_only)walker->parallel=parallel_search_new(opts,printer,multiple_targets,workers);

    for (size_t i = 0; i < opts->path_count; i++) {
        RgxWalker_Execute(walker, opts->paths[i]);
        if (opts->quiet && rgx_found) break;
    }

    parallel_search_finish(walker->parallel);walker->parallel=NULL;
    int status = opts->quiet && rgx_found ? 0 : rgx_had_error ? 2 : rgx_found ? 0 : 1;
    printer_finish(printer);
    /* Cleanup OOP objects */
    RgxWalker_Free(walker);
    RgxPrinter_Free(printer);
    matcher->vptr->Destroy(matcher);
    RgxOptions_Free(opts);

    return status;
invalid_options:
    RgxOptions_Free(opts);
    return 2;
}
