#include "app.h"
#include "symtab.h"
#include "strutil.h"

struct TypesetApp {
    SymbolTable* symbol_table;
};

static bool IsValidIdentifier(const char* str) {
    if (!str || !*str) return false;
    if (!isalpha((unsigned char)*str) && *str != '_') return false;
    for (const char* p = str + 1; *p; ++p) {
        if (!isalnum((unsigned char)*p) && *p != '_') return false;
    }
    return true;
}

static void PrintHelp(void) {
    printf("Usage: typeset [-afrituxp] [-L[n]] [-R[n]] [-Z[n]] [-i[base]] [name[=value] ...]\n");
    printf("       typeset [+afrituxp] [+L] [+R] [+Z] [+i] [name ...]\n");
    printf("Set, modify, and display variables and attributes in Windows NT.\n\n");
    printf("Attribute Qualifiers:\n");
    printf("  -a            Declare variable as an indexed array\n");
    printf("  -f            Refer to function names\n");
    printf("  -i[base]      Integer format with arithmetic evaluation; base is between 2 and 36\n");
    printf("  -l            Automatic lowercase mapping on assignment\n");
    printf("  -u            Automatic uppercase mapping on assignment\n");
    printf("  -r            Mark variable as read-only (immutable; cannot be cleared or reassigned)\n");
    printf("  -x            Export variable to Win32 process environment\n");
    printf("  -t            Tag variable for execution trace\n");
    printf("  -L[n]         Left-justify string, strip leading spaces, format to field width [n]\n");
    printf("  -R[n]         Right-justify string, format to field width [n]\n");
    printf("  -Z[n]         Zero-fill number or string, format to field width [n]\n");
    printf("  -p            Display variable attributes and declarations in parseable format\n");
    printf("  +<opt>        Unset the specified attribute flag\n");
    printf("      --help    Display this reference documentation and exit\n");
    printf("      --version Output tool version and exit\n\n");
    printf("Subcommands:\n");
    printf("  unset name... Remove variables from the symbol table\n\n");
    printf("Exit Status:\n");
    printf("  0 on success, non-zero if invalid syntax, unknown option, or read-only violation occurs.\n");
}

TypesetApp* TypesetApp_Create(void) {
    TypesetApp* app = (TypesetApp*)calloc(1, sizeof(TypesetApp));
    if (!app) return NULL;
    app->symbol_table = SymbolTable_Create();
    return app;
}

void TypesetApp_Destroy(TypesetApp* self) {
    if (self) {
        if (self->symbol_table) {
            self->symbol_table->vptr->Destroy(self->symbol_table);
        }
        free(self);
    }
}

int TypesetApp_Run(TypesetApp* self, int argc, char** argv) {
    if (argc == 1) {
        self->symbol_table->vptr->List(self->symbol_table, 0, false, stdout);
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0) {
        PrintHelp();
        return 0;
    }
    if (strcmp(argv[1], "--version") == 0) {
        printf("typeset %s\n", TYPESET_VERSION);
        return 0;
    }

    if (strcmp(argv[1], "unset") == 0) {
        if (argc < 3) {
            fprintf(stderr, "typeset unset: missing variable name\n");
            return 1;
        }
        int rc = 0;
        for (int i = 2; i < argc; ++i) {
            if (!self->symbol_table->vptr->Remove(self->symbol_table, argv[i])) {
                rc = 1;
            }
        }
        self->symbol_table->vptr->SaveState(self->symbol_table);
        return rc;
    }

    unsigned int set_flags = 0;
    unsigned int clr_flags = 0;
    int width = 0;
    int base = 10;
    bool print_only = false;
    int arg_idx = 1;

    while (arg_idx < argc && (argv[arg_idx][0] == '-' || argv[arg_idx][0] == '+')) {
        char* arg = argv[arg_idx];
        if (strcmp(arg, "--") == 0) { arg_idx++; break; }

        bool enable = (arg[0] == '-');
        size_t len = strlen(arg);

        for (size_t i = 1; i < len; ++i) {
            char opt = arg[i];
            switch (opt) {
            case 'r': if (enable) set_flags |= ATTR_READONLY; else clr_flags |= ATTR_READONLY; break;
            case 'x': if (enable) set_flags |= ATTR_EXPORT;   else clr_flags |= ATTR_EXPORT;   break;
            case 'l': if (enable) set_flags |= ATTR_LOWER;    else clr_flags |= ATTR_LOWER;    break;
            case 'u': if (enable) set_flags |= ATTR_UPPER;    else clr_flags |= ATTR_UPPER;    break;
            case 't': if (enable) set_flags |= ATTR_TAGGED;   else clr_flags |= ATTR_TAGGED;   break;
            case 'a': if (enable) set_flags |= ATTR_ARRAY;    else clr_flags |= ATTR_ARRAY;    break;
            case 'f': if (enable) set_flags |= ATTR_FUNCTION; else clr_flags |= ATTR_FUNCTION; break;
            case 'p': print_only = true; break;

            case 'i':
                if (enable) {
                    set_flags |= ATTR_INTEGER;
                    if (isdigit((unsigned char)arg[i + 1])) {
                        base = atoi(&arg[i + 1]);
                        i = len - 1;
                    }
                } else {
                    clr_flags |= ATTR_INTEGER;
                }
                break;

            case 'L':
            case 'R':
            case 'Z':
                if (enable) {
                    if (opt == 'L') set_flags |= ATTR_LJUST;
                    if (opt == 'R') set_flags |= ATTR_RJUST;
                    if (opt == 'Z') set_flags |= ATTR_ZFILL;
                    if (isdigit((unsigned char)arg[i + 1])) {
                        width = atoi(&arg[i + 1]);
                        i = len - 1;
                    }
                } else {
                    if (opt == 'L') clr_flags |= ATTR_LJUST;
                    if (opt == 'R') clr_flags |= ATTR_RJUST;
                    if (opt == 'Z') clr_flags |= ATTR_ZFILL;
                }
                break;

            default:
                fprintf(stderr, "typeset: unknown option '%c'\n", opt);
                PrintHelp();
                return 1;
            }
        }
        arg_idx++;
    }

    if (arg_idx >= argc) {
        if (print_only) {
            self->symbol_table->vptr->List(self->symbol_table, 0, false, stdout);
        } else {
            self->symbol_table->vptr->List(self->symbol_table, set_flags, false, stdout);
        }
        return 0;
    }

    int exit_status = 0;

    for (int i = arg_idx; i < argc; ++i) {
        char* token = SafeStrDup(argv[i]);
        if (!token) continue;

        char* eq = strchr(token, '=');
        char* name = token;
        char* val = NULL;

        if (eq) {
            *eq = '\0';
            val = eq + 1;
        }

        if (!IsValidIdentifier(name)) {
            fprintf(stderr, "typeset: '%s': not a valid identifier\n", name);
            free(token);
            exit_status = 1;
            continue;
        }

        if (print_only) {
            Variable* v = self->symbol_table->vptr->Get(self->symbol_table, name);
            if (v) {
                v->vptr->PrintDefinition(v, stdout);
            } else {
                fprintf(stderr, "typeset: %s: not found\n", name);
                exit_status = 1;
            }
        } else {
            if (!self->symbol_table->vptr->Set(self->symbol_table, name, val, set_flags, clr_flags, width, base)) {
                exit_status = 1;
            }
        }
        free(token);
    }

    self->symbol_table->vptr->SaveState(self->symbol_table);
    return exit_status;
}
