#ifndef LS_H
#define LS_H

#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define WINNT_LS_VERSION L"6.1.0"
#define DEFAULT_BLOCK_SIZE 512
#define KB_BLOCK_SIZE 1024
#define MAX_EXTENDED_PATH 32768
#define SID_CACHE_CAPACITY 256

typedef struct IFileRecord IFileRecord;
typedef struct IDirectoryScanner IDirectoryScanner;
typedef struct ISecurityContext ISecurityContext;
typedef struct ILsEngine ILsEngine;

typedef enum LsColorMode {
    LS_COLOR_AUTO,
    LS_COLOR_ALWAYS,
    LS_COLOR_NEVER
} LsColorMode;

typedef enum LsDaclState {
    LS_DACL_UNKNOWN,
    LS_DACL_ABSENT,
    LS_DACL_PRESENT
} LsDaclState;

typedef enum LsOutputMode {
    LS_OUTPUT_AUTO,
    LS_OUTPUT_COLUMNS,
    LS_OUTPUT_ACROSS,
    LS_OUTPUT_COMMA,
    LS_OUTPUT_SINGLE
} LsOutputMode;

typedef enum LsQuoteStyle {
    LS_QUOTE_LITERAL,
    LS_QUOTE_ESCAPE,
    LS_QUOTE_SHELL,
    LS_QUOTE_C
} LsQuoteStyle;

typedef enum LsSortMode {
    LS_SORT_NAME,
    LS_SORT_TIME,
    LS_SORT_SIZE,
    LS_SORT_EXTENSION,
    LS_SORT_VERSION
} LsSortMode;

typedef struct LsConfig {
    bool show_all;
    bool show_almost_all;
    bool recursive;
    bool disable_sort;
    bool follow_command_line_symlinks;
    bool follow_all_symlinks;
    bool group_directories_first;
    bool group_directories_last;
    bool long_format;
    bool show_acl;
    bool human_readable;
    bool show_inode;
    LsQuoteStyle quote_style;
    bool classify;
    bool dir_slash;
    bool reverse_sort;
    LsSortMode sort_mode;
    bool use_access_time;
    bool use_create_time;
    bool show_blocks;
    bool block_kb;
    bool directory_only;
    size_t output_width;
    LsOutputMode output_mode;
    LsColorMode color_mode;
} LsConfig;

typedef struct LsDirectoryIdentity {
    DWORD volume_serial;
    ULONGLONG file_id;
} LsDirectoryIdentity;

typedef struct SidCacheEntry {
    PSID sid;
    wchar_t name[128];
} SidCacheEntry;

struct ISecurityContext {
    SidCacheEntry cache[SID_CACHE_CAPACITY];
    size_t cache_count;
    void (*ResolveSid)(ISecurityContext *self, PSID sid, wchar_t *out_name, DWORD max_len);
    void (*InspectAcl)(ISecurityContext *self, HANDLE hFile, wchar_t *owner, DWORD max_owner,
                       wchar_t *group, DWORD max_group, LsDaclState *dacl_state, bool collect_acl,
                       wchar_t **acl_entries);
    void (*Destroy)(ISecurityContext *self);
};

struct IFileRecord {
    wchar_t *file_name;
    wchar_t *full_path;
    DWORD attributes;
    DWORD reparse_tag;
    ULARGE_INTEGER file_size;
    ULARGE_INTEGER allocation_size;
    FILETIME ft_creation;
    FILETIME ft_last_access;
    FILETIME ft_last_write;
    DWORD link_count;
    ULONGLONG file_id;
    DWORD volume_serial;
    bool file_id_valid;
    wchar_t windows_attributes[10];
    wchar_t owner[128];
    wchar_t group[128];
    LsDaclState dacl_state;
    wchar_t *acl_entries;
    wchar_t (*GetClassifySuffix)(const IFileRecord *self, bool slash_only);
    ULONGLONG (*GetBlockCount)(const IFileRecord *self, size_t block_size);
    time_t (*GetSelectedTime)(const IFileRecord *self, const LsConfig *cfg);
    void (*Destroy)(IFileRecord *self);
};

struct IDirectoryScanner {
    IFileRecord *records;
    size_t count;
    size_t capacity;
    bool (*Add)(IDirectoryScanner *self, const IFileRecord *record);
    void (*Sort)(IDirectoryScanner *self, const LsConfig *cfg);
    void (*Clear)(IDirectoryScanner *self);
    void (*Destroy)(IDirectoryScanner *self);
};

struct ILsEngine {
    LsConfig config;
    bool use_color;
    bool stdout_is_console;
    bool restore_console_mode;
    DWORD original_console_mode;
    LsDirectoryIdentity *visited_directories;
    size_t visited_directory_count;
    size_t visited_directory_capacity;
    ISecurityContext sec_context;
    IDirectoryScanner scanner;
    void (*ShowHelp)(const ILsEngine *self);
    bool (*ParseArgs)(ILsEngine *self, int argc, wchar_t **argv, int *opt_index);
    void (*RenderEntry)(const ILsEngine *self, const IFileRecord *record);
    int (*ProcessTarget)(ILsEngine *self, const wchar_t *target_path);
    int (*Execute)(ILsEngine *self, int argc, wchar_t **argv);
    void (*Destroy)(ILsEngine *self);
};

ISecurityContext SecurityContext_Create(void);
IFileRecord FileRecord_Create(const WIN32_FIND_DATAW *wfd, const wchar_t *parent_dir,
                              const wchar_t *full_path_override,
                              ISecurityContext *sec_ctx, const LsConfig *cfg,
                              bool command_line_target);
IDirectoryScanner DirectoryScanner_Create(void);
int LsEngine_Run(int argc, wchar_t **argv);

#endif