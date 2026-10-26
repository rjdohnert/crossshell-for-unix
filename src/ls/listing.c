#include "ls.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static ILsEngine Engine_Create(void);
static void Engine_ShowHelp(const ILsEngine *self);
static bool Engine_ParseArgs(ILsEngine *self, int argc, wchar_t **argv, int *opt_index);
static void Engine_RenderEntry(const ILsEngine *self, const IFileRecord *record);
static int Engine_ProcessTarget(ILsEngine *self, const wchar_t *target_path);
static int Engine_ProcessTargetInternal(ILsEngine *self, const wchar_t *target_path,
                                        bool show_header, bool command_line_target);
static int Engine_Execute(ILsEngine *self, int argc, wchar_t **argv);
static void Engine_Destroy(ILsEngine *self);
static void Engine_RenderEntryInternal(const ILsEngine *self, const IFileRecord *record,
                                       bool force_newline);
static void Engine_RenderDirectory(const ILsEngine *self);

static void Engine_ClearVisitedDirectories(ILsEngine *self) {
    free(self->visited_directories);
    self->visited_directories = NULL;
    self->visited_directory_count = 0;
    self->visited_directory_capacity = 0;
}

static int Engine_MarkDirectoryVisited(ILsEngine *self, const wchar_t *path,
                                       bool command_line_target) {
    bool follow_link = self->config.follow_all_symlinks ||
                       (self->config.follow_command_line_symlinks && command_line_target);
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
    if (!follow_link) flags |= FILE_FLAG_OPEN_REPARSE_POINT;
    HANDLE directory = CreateFileW(path, FILE_READ_ATTRIBUTES,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   NULL, OPEN_EXISTING, flags, NULL);
    if (directory == INVALID_HANDLE_VALUE) return 1;
    BY_HANDLE_FILE_INFORMATION info;
    bool have_identity = GetFileInformationByHandle(directory, &info) != 0;
    CloseHandle(directory);
    if (!have_identity) return 1;
    ULONGLONG file_id = ((ULONGLONG)info.nFileIndexHigh << 32) | info.nFileIndexLow;
    for (size_t i = 0; i < self->visited_directory_count; i++) {
        const LsDirectoryIdentity *identity = &self->visited_directories[i];
        if (identity->volume_serial == info.dwVolumeSerialNumber && identity->file_id == file_id) {
            return 0;
        }
    }
    if (self->visited_directory_count == self->visited_directory_capacity) {
        size_t capacity = self->visited_directory_capacity ? self->visited_directory_capacity * 2 : 16;
        if (capacity < self->visited_directory_capacity ||
            capacity > SIZE_MAX / sizeof(LsDirectoryIdentity)) return -1;
        LsDirectoryIdentity *identities = (LsDirectoryIdentity *)realloc(
            self->visited_directories, capacity * sizeof(LsDirectoryIdentity));
        if (!identities) return -1;
        self->visited_directories = identities;
        self->visited_directory_capacity = capacity;
    }
    LsDirectoryIdentity *identity = &self->visited_directories[self->visited_directory_count++];
    identity->volume_serial = info.dwVolumeSerialNumber;
    identity->file_id = file_id;
    return 1;
}

typedef struct WideNameBuffer {
    wchar_t *data;
    size_t length;
    size_t capacity;
} WideNameBuffer;

static bool WideNameBuffer_Append(WideNameBuffer *buffer, wchar_t value) {
    if (buffer->length + 1 >= buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity * 2 : 64;
        if (capacity < buffer->capacity || capacity > SIZE_MAX / sizeof(wchar_t)) return false;
        wchar_t *data = (wchar_t *)realloc(buffer->data, capacity * sizeof(wchar_t));
        if (!data) return false;
        buffer->data = data;
        buffer->capacity = capacity;
    }
    buffer->data[buffer->length++] = value;
    buffer->data[buffer->length] = L'\0';
    return true;
}

static bool WideNameBuffer_AppendText(WideNameBuffer *buffer, const wchar_t *text) {
    while (*text) {
        if (!WideNameBuffer_Append(buffer, *text++)) return false;
    }
    return true;
}

static bool Engine_AppendEscapedCharacter(WideNameBuffer *buffer, wchar_t character) {
    switch (character) {
        case L'\\': return WideNameBuffer_AppendText(buffer, L"\\\\");
        case L'\n': return WideNameBuffer_AppendText(buffer, L"\\n");
        case L'\r': return WideNameBuffer_AppendText(buffer, L"\\r");
        case L'\t': return WideNameBuffer_AppendText(buffer, L"\\t");
        case L'\b': return WideNameBuffer_AppendText(buffer, L"\\b");
        case L'\f': return WideNameBuffer_AppendText(buffer, L"\\f");
        case L'\v': return WideNameBuffer_AppendText(buffer, L"\\v");
        default: break;
    }
    if (character < 0x20 || character == 0x7f) {
        return WideNameBuffer_AppendText(buffer, L"\\x") &&
               WideNameBuffer_Append(buffer, L"0123456789ABCDEF"[(character >> 4) & 0xf]) &&
               WideNameBuffer_Append(buffer, L"0123456789ABCDEF"[character & 0xf]);
    }
    return WideNameBuffer_Append(buffer, character);
}

static wchar_t *Engine_FormatEntryName(const ILsEngine *self, const IFileRecord *record) {
    const wchar_t *name = record->file_name;
    WideNameBuffer buffer = {0};
    bool success = true;
    if (self->config.quote_style == LS_QUOTE_C) success = WideNameBuffer_Append(&buffer, L'"');
    else if (self->config.quote_style == LS_QUOTE_SHELL) success = WideNameBuffer_Append(&buffer, L'\'');

    for (size_t i = 0; success && name[i]; i++) {
        wchar_t character = name[i];
        if (self->config.quote_style == LS_QUOTE_C) {
            if (character == L'"') success = WideNameBuffer_AppendText(&buffer, L"\\\"");
            else success = Engine_AppendEscapedCharacter(&buffer, character);
        } else if (self->config.quote_style == LS_QUOTE_ESCAPE) {
            if (character == L' ' || character == L'\'' || character == L'"' ||
                character == L'(' || character == L')' ||
                character == L'[' || character == L']' || character == L'{' ||
                character == L'}' || character == L'$' || character == L'&' ||
                character == L';' || character == L'|' || character == L'<' ||
                character == L'>' || character == L'`' || character == L'!' ||
                character == L'*' || character == L'?') {
                success = WideNameBuffer_Append(&buffer, L'\\');
            }
            if (success) success = Engine_AppendEscapedCharacter(&buffer, character);
        } else if (self->config.quote_style == LS_QUOTE_SHELL && character == L'\'') {
            success = WideNameBuffer_AppendText(&buffer, L"'\\''");
        } else {
            success = WideNameBuffer_Append(&buffer, character);
        }
    }

    if (success && self->config.quote_style == LS_QUOTE_C) success = WideNameBuffer_Append(&buffer, L'"');
    else if (success && self->config.quote_style == LS_QUOTE_SHELL) success = WideNameBuffer_Append(&buffer, L'\'');
    if (!success) {
        free(buffer.data);
        return NULL;
    }
    if (!buffer.data) return _wcsdup(L"");
    return buffer.data;
}

static size_t Engine_GetDisplayWidth(const wchar_t *text) {
    size_t width = 0;
    for (size_t i = 0; text[i]; i++) {
        if (text[i] >= 0xd800 && text[i] <= 0xdbff &&
            text[i + 1] >= 0xdc00 && text[i + 1] <= 0xdfff) {
            width += 2;
            i++;
            continue;
        }
        WORD character_type = 0;
        if (GetStringTypeW(CT_CTYPE3, &text[i], 1, &character_type) &&
            (character_type & (C3_NONSPACING | C3_DIACRITIC | C3_VOWELMARK))) continue;
        if (character_type & (C3_FULLWIDTH | C3_IDEOGRAPH)) width += 2;
        else width++;
    }
    return width;
}

static void Engine_FormatSize(ULONGLONG size, bool human_readable,
                              wchar_t *out, size_t capacity) {
    static const wchar_t units[] = L"BKMGTPE";
    if (!human_readable) {
        _snwprintf_s(out, capacity, _TRUNCATE, L"%llu", (unsigned long long)size);
        return;
    }
    double value = (double)size;
    size_t unit = 0;
    while (value >= 1024.0 && unit < (sizeof(units) / sizeof(units[0])) - 2) {
        value /= 1024.0;
        unit++;
    }
    if (unit == 0) {
        _snwprintf_s(out, capacity, _TRUNCATE, L"%llu", (unsigned long long)size);
    } else {
        _snwprintf_s(out, capacity, _TRUNCATE, L"%.1f%c", value, units[unit]);
    }
}

static bool Engine_HasExtension(const wchar_t *file_name, const wchar_t *const *extensions,
                                size_t extension_count) {
    const wchar_t *extension = wcsrchr(file_name, L'.');
    if (!extension) return false;
    for (size_t i = 0; i < extension_count; i++) {
        if (_wcsicmp(extension, extensions[i]) == 0) return true;
    }
    return false;
}

static const wchar_t *Engine_GetEntryColor(const ILsEngine *self, const IFileRecord *record) {
    (void)self;
    static const wchar_t *const installer_extensions[] = {
        L".appinstaller", L".appx", L".appxbundle", L".msi", L".msix",
        L".msixbundle", L".msp", L".msu"
    };
    static const wchar_t *const image_extensions[] = {
        L".apng", L".avif", L".bmp", L".gif", L".heic", L".heif", L".ico",
        L".jpeg", L".jpg", L".png", L".psd", L".svg", L".tif", L".tiff", L".webp"
    };
    static const wchar_t *const media_extensions[] = {
        L".3gp", L".aac", L".aiff", L".alac", L".avi", L".flac", L".m4a", L".m4v",
        L".mkv", L".mov", L".mp3", L".mp4", L".mpeg", L".mpg", L".ogg", L".opus",
        L".wav", L".weba", L".webm", L".wma", L".wmv"
    };
    static const wchar_t *const archive_extensions[] = {
        L".7z", L".arj", L".bz", L".bz2", L".cab", L".cpio", L".deb", L".gz",
        L".iso", L".lha", L".lzh", L".lz", L".lz4", L".lzma", L".lzo", L".rar",
        L".rpm", L".tar", L".tgz", L".txz", L".war", L".wim", L".xar", L".xz",
        L".z", L".zip", L".zst", L".zstd"
    };
    static const wchar_t *const source_extensions[] = {
        L".asm", L".bat", L".c", L".cc", L".cfg", L".cmd", L".cpp", L".cs", L".css",
        L".cxx", L".go", L".h", L".hpp", L".hxx", L".html", L".java", L".js", L".json",
        L".jsx", L".kt", L".lua", L".m", L".md", L".mm", L".php", L".pl", L".ps1",
        L".py", L".r", L".rb", L".rs", L".sass", L".scala", L".scss", L".sh", L".sql",
        L".swift", L".toml", L".ts", L".tsx", L".xml", L".yaml", L".yml", L".zsh"
    };
    static wchar_t custom_color[64];

    wchar_t ls_colors[4096];
    DWORD ls_colors_length = GetEnvironmentVariableW(L"LS_COLORS", ls_colors,
                                                       sizeof(ls_colors) / sizeof(wchar_t));
    if (ls_colors_length > 0 && ls_colors_length < sizeof(ls_colors) / sizeof(wchar_t)) {
        wchar_t extension[64];
        const wchar_t *file_extension = wcsrchr(record->file_name, L'.');
        if (file_extension) wcsncpy_s(extension, 64, file_extension, _TRUNCATE);
        else extension[0] = L'\0';
        const wchar_t *cursor = ls_colors;
        while (*cursor) {
            const wchar_t *separator = wcschr(cursor, L':');
            size_t entry_length = separator ? (size_t)(separator - cursor) : wcslen(cursor);
            const wchar_t *equals = wmemchr(cursor, L'=', entry_length);
            if (equals && equals < cursor + entry_length) {
                size_t key_length = (size_t)(equals - cursor);
                bool match = (record->attributes & FILE_ATTRIBUTE_DIRECTORY)
                    ? (key_length == 2 && wcsncmp(cursor, L"di", 2) == 0)
                                  : (key_length == wcslen(extension) + 1 && cursor[0] == L'*' &&
                                      cursor[1] == L'.' &&
                                      _wcsnicmp(cursor + 1, extension, wcslen(extension)) == 0);
                size_t code_length = (size_t)(cursor + entry_length - equals - 1);
                bool valid_code = code_length > 0 && code_length < 48;
                for (size_t i = 0; valid_code && i < code_length; i++) {
                    wchar_t character = equals[1 + i];
                    if (!((character >= L'0' && character <= L'9') || character == L';')) {
                        valid_code = false;
                    }
                }
                if (match && valid_code) {
                    _snwprintf_s(custom_color, 64, _TRUNCATE, L"\x1b[%.*sm",
                                 (int)code_length, equals + 1);
                    return custom_color;
                }
            }
            if (!separator) break;
            cursor = separator + 1;
        }
    }

    if (record->attributes & FILE_ATTRIBUTE_DIRECTORY) return L"\x1b[94m";
    if (Engine_HasExtension(record->file_name, installer_extensions,
                            sizeof(installer_extensions) / sizeof(installer_extensions[0]))) {
        return L"\x1b[96m";
    }
    if (Engine_HasExtension(record->file_name, image_extensions,
                            sizeof(image_extensions) / sizeof(image_extensions[0]))) {
        return L"\x1b[38;5;213m";
    }
    if (Engine_HasExtension(record->file_name, media_extensions,
                            sizeof(media_extensions) / sizeof(media_extensions[0]))) {
        return L"\x1b[35m";
    }
    if (Engine_HasExtension(record->file_name, archive_extensions,
                            sizeof(archive_extensions) / sizeof(archive_extensions[0]))) {
        return L"\x1b[91m";
    }
    if (Engine_HasExtension(record->file_name, source_extensions,
                            sizeof(source_extensions) / sizeof(source_extensions[0]))) {
        return L"\x1b[38;5;208m";
    }
    return NULL;
}

static bool Engine_EnableColor(ILsEngine *self) {
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    self->stdout_is_console = GetConsoleMode(output, &mode) != 0;
    if (self->config.color_mode == LS_COLOR_NEVER) return false;
    if (self->config.color_mode == LS_COLOR_AUTO) {
        wchar_t no_color[2];
        if (GetEnvironmentVariableW(L"NO_COLOR", no_color, 2) > 0) return false;
    }
    if (!self->stdout_is_console) return self->config.color_mode == LS_COLOR_ALWAYS;
    if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) return true;
    if (!SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        return self->config.color_mode == LS_COLOR_ALWAYS;
    }
    self->original_console_mode = mode;
    self->restore_console_mode = true;
    return true;
}

static void Engine_ShowHelp(const ILsEngine *self) {
    (void)self;
    wprintf(L"Usage: ls [options] [File...]\n");
    wprintf(L"Directory Lister for Windows (Release %s)\n\n", WINNT_LS_VERSION);
    wprintf(L"Standard Windows Listing Flags:\n");
    wprintf(L"  -a          List all entries including '.', and '..' and hidden attributes.\n");
    wprintf(L"  -A          List all entries except '.', and '..'.\n");
    wprintf(L"  -c          Use time of creation for sorting (-t) or listing (-l).\n");
    wprintf(L"  -C          Display entries in vertical columns.\n");
    wprintf(L"  -d          Display directory names instead of contents.\n");
    wprintf(L"  -e          Compatibility alias for --acl (also enables long format).\n");
    wprintf(L"  -F          Classify: '/' for dirs, '*' for executables, '@' for junctions/symlinks.\n");
    wprintf(L"  -H          Follow command-line symbolic links with -R.\n");
    wprintf(L"  -h          Human-readable sizes in long format (base 1024).\n");
    wprintf(L"  -i          Display the Windows file ID before each entry.\n");
    wprintf(L"  -f          Include hidden entries and disable sorting.\n");
    wprintf(L"  -b          Escape shell-special and non-printing characters.\n");
    wprintf(L"  -k          Display allocated block units in 1024-byte (1KB) blocks.\n");
    wprintf(L"  -l          Long format: Windows attribute mask, owner, size, and timestamp.\n");
    wprintf(L"  -m          Stream entries as a comma-separated list.\n");
    wprintf(L"  -L          Follow symbolic links; recursive traversal detects cycles.\n");
    wprintf(L"  -N          Print names literally without quoting.\n");
    wprintf(L"  --acl       List each DACL trustee's allow/deny rwx rights.\n");
    wprintf(L"  --color     Force colored output.\n");
    wprintf(L"  --no-color  Disable colored output.\n");
    wprintf(L"  --width=N   Set output width for column layout.\n");
    wprintf(L"  -p          Append a slash (/) after each directory name.\n");
    wprintf(L"  -P          Do not follow symbolic links (default).\n");
    wprintf(L"  -r          Reverse the order of sorting.\n");
    wprintf(L"  -R          List directories recursively.\n");
    wprintf(L"  -Q          Quote names using C-style escapes.\n");
    wprintf(L"  -s          Display allocation size in blocks (512B default, 1024B with -k).\n");
    wprintf(L"  -S          Sort by file size descending.\n");
    wprintf(L"  -v          Natural/version sort.\n");
    wprintf(L"  -X          Sort by file extension.\n");
    wprintf(L"  --group-directories-first  Group directories before files.\n");
    wprintf(L"  --group-directories-last   Group directories after files.\n");
    wprintf(L"  -t          Sort by last modification timestamp descending.\n");
    wprintf(L"  -u          Use time of last access for sorting (-t) or listing (-l).\n");
    wprintf(L"  -1          Output one entry per line.\n");
    wprintf(L"  -x          Display entries in horizontal rows.\n");
    wprintf(L"  --quoting-style=STYLE  literal, escape, shell, or c.\n");
    wprintf(L"  -? / --help  Windows command usage reference.\n\n");
    wprintf(L"Native Windows Attribute Mask (-l, 9 characters):\n");
    wprintf(L"  d r a h s c e l + = directory, read-only, archive, hidden, system,\n");
    wprintf(L"                    compressed, encrypted, reparse point, DACL present.\n");
    wprintf(L"  '*'         Executable file extensions (.exe, .cmd, .bat, .ps1)\n");
}

static bool Engine_ParseArgs(ILsEngine *self, int argc, wchar_t **argv, int *opt_index) {
    int i = 1;
    for (; i < argc; i++) {
        if (argv[i][0] != L'-' || wcscmp(argv[i], L"-") == 0) break;
        if (wcscmp(argv[i], L"--") == 0) {
            i++;
            break;
        }
        if (wcscmp(argv[i], L"--help") == 0 || wcscmp(argv[i], L"-?") == 0) {
            self->ShowHelp(self);
            exit(0);
        }
        if (wcscmp(argv[i], L"--acl") == 0) {
            self->config.show_acl = true;
            continue;
        }
        if (wcscmp(argv[i], L"--group-directories-first") == 0) {
            self->config.group_directories_first = true;
            self->config.group_directories_last = false;
            continue;
        }
        if (wcscmp(argv[i], L"--group-directories-last") == 0) {
            self->config.group_directories_first = false;
            self->config.group_directories_last = true;
            continue;
        }
        if (wcscmp(argv[i], L"--color") == 0) {
            self->config.color_mode = LS_COLOR_ALWAYS;
            continue;
        }
        if (wcscmp(argv[i], L"--no-color") == 0) {
            self->config.color_mode = LS_COLOR_NEVER;
            continue;
        }
        if (wcsncmp(argv[i], L"--width=", 8) == 0) {
            wchar_t *end = NULL;
            unsigned long width = wcstoul(argv[i] + 8, &end, 10);
            if (!end || *end != L'\0' || width == 0 || width > 32768) {
                fwprintf(stderr, L"ls: invalid output width: %s\n", argv[i] + 8);
                return false;
            }
            self->config.output_width = (size_t)width;
            continue;
        }
        if (wcsncmp(argv[i], L"--quoting-style=", 16) == 0) {
            const wchar_t *style = argv[i] + 16;
            if (wcscmp(style, L"literal") == 0) self->config.quote_style = LS_QUOTE_LITERAL;
            else if (wcscmp(style, L"escape") == 0) self->config.quote_style = LS_QUOTE_ESCAPE;
            else if (wcscmp(style, L"shell") == 0) self->config.quote_style = LS_QUOTE_SHELL;
            else if (wcscmp(style, L"c") == 0) self->config.quote_style = LS_QUOTE_C;
            else {
                fwprintf(stderr, L"ls: unsupported quoting style: %s\n", style);
                return false;
            }
            continue;
        }
        for (int j = 1; argv[i][j] != L'\0'; j++) {
            switch (argv[i][j]) {
                case L'a': self->config.show_all = true; break;
                case L'A': self->config.show_almost_all = true; break;
                case L'b': self->config.quote_style = LS_QUOTE_ESCAPE; break;
                case L'C': self->config.output_mode = LS_OUTPUT_COLUMNS; break;
                case L'l': self->config.long_format = true; break;
                case L'e': self->config.long_format = true; self->config.show_acl = true; break;
                case L'f': self->config.show_all = true; self->config.disable_sort = true; break;
                case L'F': self->config.classify = true; break;
                case L'H':
                    self->config.follow_command_line_symlinks = true;
                    self->config.follow_all_symlinks = false;
                    break;
                case L'h': self->config.human_readable = true; break;
                case L'i': self->config.show_inode = true; break;
                case L'L':
                    self->config.follow_all_symlinks = true;
                    self->config.follow_command_line_symlinks = false;
                    break;
                case L'N': self->config.quote_style = LS_QUOTE_LITERAL; break;
                case L'Q': self->config.quote_style = LS_QUOTE_C; break;
                case L'm': self->config.output_mode = LS_OUTPUT_COMMA; break;
                case L'p': self->config.dir_slash = true; break;
                case L'P':
                    self->config.follow_all_symlinks = false;
                    self->config.follow_command_line_symlinks = false;
                    break;
                case L'r': self->config.reverse_sort = true; break;
                case L'R': self->config.recursive = true; break;
                case L't': self->config.sort_mode = LS_SORT_TIME; break;
                case L'S': self->config.sort_mode = LS_SORT_SIZE; break;
                case L'v': self->config.sort_mode = LS_SORT_VERSION; break;
                case L'X': self->config.sort_mode = LS_SORT_EXTENSION; break;
                case L'u': self->config.use_access_time = true; break;
                case L'c': self->config.use_create_time = true; break;
                case L's': self->config.show_blocks = true; break;
                case L'k': self->config.block_kb = true; break;
                case L'1': self->config.output_mode = LS_OUTPUT_SINGLE; break;
                case L'd': self->config.directory_only = true; break;
                case L'x': self->config.output_mode = LS_OUTPUT_ACROSS; break;
                default:
                    fwprintf(stderr, L"ls: 0653-348 Not a recognized flag: %c\n", argv[i][j]);
                    fwprintf(stderr, L"Type 'ls -?' for comprehensive Windows usage instructions.\n");
                    return false;
            }
        }
    }
    *opt_index = i;
    return true;
}

static wchar_t Engine_GetEntrySuffix(const ILsEngine *self, const IFileRecord *record) {
    return record->GetClassifySuffix(
        record, self->config.dir_slash && !self->config.classify);
}

static size_t Engine_GetEntryWidth(const ILsEngine *self, const IFileRecord *record) {
    wchar_t *formatted_name = Engine_FormatEntryName(self, record);
    if (!formatted_name) return wcslen(record->file_name);
    size_t width = Engine_GetDisplayWidth(formatted_name);
    free(formatted_name);
    if (Engine_GetEntrySuffix(self, record) != L'\0' &&
        (self->config.classify || self->config.dir_slash)) width++;
    return width;
}

static void Engine_WriteEntryName(const ILsEngine *self, const IFileRecord *record) {
    wchar_t *formatted_name = Engine_FormatEntryName(self, record);
    if (!formatted_name) formatted_name = _wcsdup(record->file_name);
    if (!formatted_name) return;
    wchar_t suffix = Engine_GetEntrySuffix(self, record);
    const wchar_t *color = self->use_color ? Engine_GetEntryColor(self, record) : NULL;
    if (color) wprintf(L"%s", color);
    if (suffix != L'\0' && (self->config.classify || self->config.dir_slash)) {
        wprintf(L"%s%c", formatted_name, suffix);
    } else {
        wprintf(L"%s", formatted_name);
    }
    if (color) wprintf(L"\x1b[0m");
    free(formatted_name);
}

static void Engine_RenderEntryInternal(const ILsEngine *self, const IFileRecord *record,
                                       bool force_newline) {
    if (self->config.show_inode) {
        if (record->file_id_valid) wprintf(L"%llu ", (unsigned long long)record->file_id);
        else wprintf(L"? ");
    }
    if (self->config.show_blocks) {
        size_t block_size = self->config.block_kb ? KB_BLOCK_SIZE : DEFAULT_BLOCK_SIZE;
        wprintf(L"%4llu ", (unsigned long long)record->GetBlockCount(record, block_size));
    }
    if (self->config.long_format) {
        time_t selected_time = record->GetSelectedTime(record, &self->config);
        struct tm time_info;
        localtime_s(&time_info, &selected_time);
        wchar_t time_text[32];
        wcsftime(time_text, sizeof(time_text) / sizeof(wchar_t), L"%b %d %H:%M", &time_info);
        wchar_t size_text[32];
        Engine_FormatSize(record->file_size.QuadPart, self->config.human_readable,
                  size_text, sizeof(size_text) / sizeof(wchar_t));
        wprintf(L"%s %s %s %s ", record->windows_attributes, record->owner,
            size_text, time_text);
    }
    Engine_WriteEntryName(self, record);
    bool one_per_line = force_newline || self->config.long_format || self->config.show_blocks ||
                        self->config.show_acl ||
                        self->config.output_mode == LS_OUTPUT_SINGLE ||
                        (self->config.output_mode == LS_OUTPUT_AUTO && !self->stdout_is_console);
    wprintf(one_per_line ? L"\n" : L"  ");
    if (self->config.show_acl && record->acl_entries) wprintf(L"%s", record->acl_entries);
}

static void Engine_RenderEntry(const ILsEngine *self, const IFileRecord *record) {
    Engine_RenderEntryInternal(self, record, true);
}

static void Engine_RenderDirectory(const ILsEngine *self) {
    size_t count = self->scanner.count;
    LsOutputMode output_mode = self->config.output_mode;
    if (output_mode == LS_OUTPUT_AUTO) {
        output_mode = self->stdout_is_console ? LS_OUTPUT_COLUMNS : LS_OUTPUT_SINGLE;
    }
    bool force_lines = self->config.long_format || self->config.show_blocks ||
                       self->config.show_acl || output_mode == LS_OUTPUT_SINGLE;
    if (force_lines || count == 0) {
        for (size_t i = 0; i < count; i++) {
            Engine_RenderEntryInternal(self, &self->scanner.records[i], true);
        }
        return;
    }

    if (output_mode == LS_OUTPUT_COMMA) {
        for (size_t i = 0; i < count; i++) {
            Engine_WriteEntryName(self, &self->scanner.records[i]);
            wprintf(i + 1 < count ? L", " : L"\n");
        }
        return;
    }

    size_t maximum_name_width = 0;
    for (size_t i = 0; i < count; i++) {
        size_t width = Engine_GetEntryWidth(self, &self->scanner.records[i]);
        if (width > maximum_name_width) maximum_name_width = width;
    }
    CONSOLE_SCREEN_BUFFER_INFO console_info;
    size_t screen_width = self->config.output_width ? self->config.output_width : 80;
    if (!self->config.output_width &&
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &console_info)) {
        screen_width = (size_t)(console_info.srWindow.Right - console_info.srWindow.Left + 1);
    }
    size_t column_width = maximum_name_width + 2;
    size_t columns = column_width ? screen_width / column_width : 1;
    if (columns == 0) columns = 1;
    if (columns > count) columns = count;

    if (output_mode == LS_OUTPUT_COLUMNS) {
        size_t rows = (count + columns - 1) / columns;
        for (size_t row = 0; row < rows; row++) {
            for (size_t column = 0; column < columns; column++) {
                size_t index = column * rows + row;
                if (index >= count) continue;
                Engine_WriteEntryName(self, &self->scanner.records[index]);
                size_t next_index = (column + 1) * rows + row;
                if (next_index < count) {
                    size_t padding = column_width - Engine_GetEntryWidth(
                        self, &self->scanner.records[index]);
                    while (padding--) wprintf(L" ");
                }
            }
            wprintf(L"\n");
        }
    } else {
        for (size_t i = 0; i < count; i++) {
            Engine_WriteEntryName(self, &self->scanner.records[i]);
            if (i + 1 == count) {
                wprintf(L"\n");
            } else if ((i + 1) % columns == 0) {
                wprintf(L"\n");
            } else {
                size_t padding = column_width - Engine_GetEntryWidth(
                    self, &self->scanner.records[i]);
                while (padding--) wprintf(L" ");
            }
        }
    }
}

static int Engine_ProcessTargetInternal(ILsEngine *self, const wchar_t *target_path,
                                        bool show_header, bool command_line_target) {
    wchar_t normalized[MAX_EXTENDED_PATH];
    DWORD path_len = GetFullPathNameW(target_path, MAX_EXTENDED_PATH, normalized, NULL);
    if (path_len == 0 || path_len >= MAX_EXTENDED_PATH) {
        fwprintf(stderr, L"ls: 0653-341 The file %s does not exist.\n", target_path);
        return 1;
    }
    bool has_wildcards = wcspbrk(normalized, L"*?") != NULL;
    DWORD attributes = has_wildcards ? INVALID_FILE_ATTRIBUTES : GetFileAttributesW(normalized);
    if (!has_wildcards && attributes == INVALID_FILE_ATTRIBUTES) {
        fwprintf(stderr, L"ls: 0653-341 The file %s does not exist.\n", target_path);
        return 1;
    }

    bool is_directory = !has_wildcards && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    bool is_reparse_point = !has_wildcards && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    bool follow_target_link = self->config.follow_all_symlinks ||
                              (self->config.follow_command_line_symlinks && command_line_target);
    if (!has_wildcards && (!is_directory || self->config.directory_only ||
                           (is_reparse_point && !follow_target_link))) {
        WIN32_FIND_DATAW data;
        HANDLE find = FindFirstFileW(normalized, &data);
        if (find != INVALID_HANDLE_VALUE) {
            IFileRecord record = FileRecord_Create(&data, NULL, normalized,
                                                   &self->sec_context, &self->config,
                                                   command_line_target);
            if (!record.file_name || !record.full_path) {
                record.Destroy(&record);
                FindClose(find);
                fwprintf(stderr, L"ls: out of memory while reading %s.\n", target_path);
                return 1;
            }
            self->RenderEntry(self, &record);
            record.Destroy(&record);
            FindClose(find);
            return 0;
        }
    }

    wchar_t search_pattern[MAX_EXTENDED_PATH];
    wchar_t wildcard_parent[MAX_EXTENDED_PATH];
    const wchar_t *record_parent = normalized;
    if (has_wildcards) {
        const wchar_t *last_separator = wcsrchr(normalized, L'\\');
        if (!last_separator) {
            fwprintf(stderr, L"ls: invalid wildcard path %s.\n", target_path);
            return 1;
        }
        size_t parent_length = (size_t)(last_separator - normalized);
        if (parent_length >= MAX_EXTENDED_PATH) {
            fwprintf(stderr, L"ls: path is too long: %s.\n", target_path);
            return 1;
        }
        memcpy(wildcard_parent, normalized, parent_length * sizeof(wchar_t));
        wildcard_parent[parent_length] = L'\0';
        record_parent = wildcard_parent;
        wcsncpy_s(search_pattern, MAX_EXTENDED_PATH, normalized, _TRUNCATE);
    } else if (wcsncmp(normalized, L"\\\\?\\", 4) != 0) {
        _snwprintf_s(search_pattern, MAX_EXTENDED_PATH, _TRUNCATE,
                     L"\\\\?\\%s\\*", normalized);
    } else {
        _snwprintf_s(search_pattern, MAX_EXTENDED_PATH, _TRUNCATE, L"%s\\*", normalized);
    }
    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileExW(search_pattern, FindExInfoBasic, &data,
                                   FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (find == INVALID_HANDLE_VALUE) {
        if (has_wildcards) {
            fwprintf(stderr, L"ls: no files match %s.\n", target_path);
        } else {
            fwprintf(stderr, L"ls: 0653-345 Cannot open directory %s.\n", target_path);
        }
        return 1;
    }

    self->scanner.Clear(&self->scanner);
    do {
        bool is_dot = wcscmp(data.cFileName, L".") == 0;
        bool is_dotdot = wcscmp(data.cFileName, L"..") == 0;
        bool is_hidden = (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) || data.cFileName[0] == L'.';
        if (has_wildcards && self->config.directory_only &&
            !(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (!self->config.show_all) {
            if (self->config.show_almost_all) {
                if (is_dot || is_dotdot) continue;
            } else if (is_hidden) {
                continue;
            }
        }
        IFileRecord record = FileRecord_Create(&data, record_parent, NULL,
                                               &self->sec_context, &self->config,
                                               has_wildcards);
        if (!record.file_name || !record.full_path) {
            record.Destroy(&record);
            FindClose(find);
            self->scanner.Clear(&self->scanner);
            fwprintf(stderr, L"ls: out of memory while reading directory %s.\n", target_path);
            return 1;
        }
        if (!self->scanner.Add(&self->scanner, &record)) {
            record.Destroy(&record);
            FindClose(find);
            self->scanner.Clear(&self->scanner);
            fwprintf(stderr, L"ls: out of memory while scanning directory %s.\n", target_path);
            return 1;
        }
    } while (FindNextFileW(find, &data));
    DWORD scan_error = GetLastError();
    FindClose(find);
    if (scan_error != ERROR_NO_MORE_FILES) {
        self->scanner.Clear(&self->scanner);
        fwprintf(stderr, L"ls: error while reading directory %s (Windows error %lu).\n",
                 target_path, (unsigned long)scan_error);
        return 1;
    }

    self->scanner.Sort(&self->scanner, &self->config);
    if (self->config.recursive && is_directory) {
        int visited = Engine_MarkDirectoryVisited(self, normalized, command_line_target);
        if (visited == 0) {
            self->scanner.Clear(&self->scanner);
            return 0;
        }
        if (visited < 0) {
            self->scanner.Clear(&self->scanner);
            fwprintf(stderr, L"ls: unable to track recursive directory %s.\n", target_path);
            return 1;
        }
    }
    wchar_t **child_directories = NULL;
    size_t child_count = 0;
    size_t child_capacity = 0;
    if (self->config.recursive) {
        for (size_t i = 0; i < self->scanner.count; i++) {
            const IFileRecord *record = &self->scanner.records[i];
            if (!(record->attributes & FILE_ATTRIBUTE_DIRECTORY) ||
                ((record->attributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
                 !self->config.follow_all_symlinks) ||
                wcscmp(record->file_name, L".") == 0 ||
                wcscmp(record->file_name, L"..") == 0) continue;
            if (child_count == child_capacity) {
                size_t capacity = child_capacity ? child_capacity * 2 : 16;
                if (capacity < child_capacity || capacity > SIZE_MAX / sizeof(wchar_t *)) {
                    capacity = 0;
                }
                wchar_t **resized = capacity ? (wchar_t **)realloc(
                    child_directories, capacity * sizeof(wchar_t *)) : NULL;
                if (!resized) {
                    for (size_t j = 0; j < child_count; j++) free(child_directories[j]);
                    free(child_directories);
                    self->scanner.Clear(&self->scanner);
                    fwprintf(stderr, L"ls: out of memory while recursing into %s.\n", target_path);
                    return 1;
                }
                child_directories = resized;
                child_capacity = capacity;
            }
            child_directories[child_count] = _wcsdup(record->full_path);
            if (!child_directories[child_count]) {
                for (size_t j = 0; j < child_count; j++) free(child_directories[j]);
                free(child_directories);
                self->scanner.Clear(&self->scanner);
                fwprintf(stderr, L"ls: out of memory while recursing into %s.\n", target_path);
                return 1;
            }
            child_count++;
        }
    }

    if (show_header && (is_directory || has_wildcards)) wprintf(L"%s:\n", target_path);
    if (self->config.long_format && self->scanner.count > 0) {
        ULONGLONG total = 0;
        size_t block_size = self->config.block_kb ? KB_BLOCK_SIZE : DEFAULT_BLOCK_SIZE;
        for (size_t i = 0; i < self->scanner.count; i++) {
            total += self->scanner.records[i].GetBlockCount(&self->scanner.records[i], block_size);
        }
        wprintf(L"total %llu\n", (unsigned long long)total);
    }
    Engine_RenderDirectory(self);
    int status = 0;
    for (size_t i = 0; i < child_count; i++) {
        wprintf(L"\n");
        if (Engine_ProcessTargetInternal(self, child_directories[i], true, false) != 0) status = 1;
        free(child_directories[i]);
    }
    free(child_directories);
    return status;
}

static int Engine_ProcessTarget(ILsEngine *self, const wchar_t *target_path) {
    Engine_ClearVisitedDirectories(self);
    return Engine_ProcessTargetInternal(self, target_path, self->config.recursive, true);
}

static int Engine_Execute(ILsEngine *self, int argc, wchar_t **argv) {
    int opt_index = 1;
    if (!self->ParseArgs(self, argc, argv, &opt_index)) return 2;
    self->use_color = Engine_EnableColor(self);
    if (opt_index >= argc) return self->ProcessTarget(self, L".");
    int status = 0;
    int targets = argc - opt_index;
    for (int i = opt_index; i < argc; i++) {
        if (targets > 1 && !self->config.recursive) wprintf(L"%s:\n", argv[i]);
        int result = self->ProcessTarget(self, argv[i]);
        if (result != 0) status = result;
        if (targets > 1 && i < argc - 1) wprintf(L"\n");
    }
    return status;
}

static void Engine_Destroy(ILsEngine *self) {
    if (self->restore_console_mode) {
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), self->original_console_mode);
    }
    Engine_ClearVisitedDirectories(self);
    self->scanner.Destroy(&self->scanner);
    self->sec_context.Destroy(&self->sec_context);
}

static ILsEngine Engine_Create(void) {
    ILsEngine engine;
    memset(&engine, 0, sizeof(engine));
    engine.sec_context = SecurityContext_Create();
    engine.scanner = DirectoryScanner_Create();
    engine.ShowHelp = Engine_ShowHelp;
    engine.ParseArgs = Engine_ParseArgs;
    engine.RenderEntry = Engine_RenderEntry;
    engine.ProcessTarget = Engine_ProcessTarget;
    engine.Execute = Engine_Execute;
    engine.Destroy = Engine_Destroy;
    return engine;
}

int LsEngine_Run(int argc, wchar_t **argv) {
    ILsEngine engine = Engine_Create();
    int status = engine.Execute(&engine, argc, argv);
    engine.Destroy(&engine);
    return status;
}