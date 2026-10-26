#include "directory_walker.h"
#include "allocation.h"
#include "search_state.h"
#include "path_conversion.h"
#include "file_search.h"
#include "file_types.h"

RgxWalker* RgxWalker_New(const RgxOptions* opts, IMatcher* matcher, RgxPrinter* printer, bool multiple_targets) {
    RgxWalker* walker = (RgxWalker*)rgx_calloc(1, sizeof(RgxWalker)); walker->opts = opts; walker->matcher = matcher; walker->printer = printer; walker->multiple_targets = multiple_targets; return walker;
}

static void walker_file(RgxWalker* walker, const char* path) {
    if(!file_type_selected(walker->opts,path))return;
    if (walker->opts->files_only) { rgx_found = true; if (!walker->opts->quiet) RgxPrinter_PrintPath(walker->printer, path); }
    else if(walker->parallel)parallel_search_submit(walker->parallel,path);
    else SearchSingleFile(path, walker->opts, walker->matcher, walker->printer, walker->multiple_targets);
}

typedef struct { char* path; size_t depth; } WalkTask;

void RgxWalker_Execute(RgxWalker* walker, const char* target_path) {
    if (strcmp(target_path, "-") == 0) { if (!walker->opts->files_only) SearchSingleFile("-", walker->opts, walker->matcher, walker->printer, walker->multiple_targets); return; }
    wchar_t* wide = path_to_wide(target_path); if (!wide) return;
    DWORD attributes = GetFileAttributesW(wide); free(wide);
    if (attributes == INVALID_FILE_ATTRIBUTES) { fprintf(stderr, "rgx: cannot access '%s' (Windows error %lu)\n", target_path, (unsigned long)GetLastError()); rgx_had_error = true; return; }
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) { walker_file(walker, target_path); return; }
    char* root = absolute_utf8(target_path); if (!root) return;
    ignore_load_ancestors(walker, root);
    WalkTask* tasks = (WalkTask*)rgx_malloc(sizeof(WalkTask)); size_t count = 1;
    tasks[0] = (WalkTask){rgx_strdup(target_path), 0};
    size_t queue_bytes = strlen(target_path) + 1;
    while (count && !(walker->opts->quiet && rgx_found)) {
        WalkTask task = tasks[--count];
        queue_bytes -= strlen(task.path) + 1;
        char* scope = absolute_utf8(task.path); if (scope) { ignore_load_directory(walker, scope); free(scope); }
        if (walker->opts->max_depth_set && task.depth >= walker->opts->max_depth) { free(task.path); continue; }
        char* wildcard = path_join(task.path, "*"); wide = wildcard ? path_to_wide(wildcard) : NULL; free(wildcard);
        WIN32_FIND_DATAW data; HANDLE find = wide ? FindFirstFileW(wide, &data) : INVALID_HANDLE_VALUE; free(wide);
        if (find == INVALID_HANDLE_VALUE) { DWORD error = GetLastError(); if (error != ERROR_FILE_NOT_FOUND) { fprintf(stderr, "rgx: cannot enumerate '%s' (Windows error %lu)\n", task.path, (unsigned long)error); rgx_had_error = true; } free(task.path); continue; }
        do {
            if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
            if (!walker->opts->search_hidden && ((data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) || data.cFileName[0] == L'.')) continue;
            if (!walker->opts->no_ignore && (wcscmp(data.cFileName, L".git") == 0 || wcscmp(data.cFileName, L".hg") == 0 || wcscmp(data.cFileName, L".svn") == 0)) continue;
            if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
            char* name = wide_to_utf8(data.cFileName); if (!name) continue;
            char* path = path_join(task.path, name); free(name); if (!path) continue;
            char* absolute = absolute_utf8(path); bool directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            bool ignored = !absolute || walker_ignored(walker, absolute, directory, root); free(absolute);
            if (ignored) { free(path); continue; }
            if (directory) {
                size_t cost = strlen(path) + 1;
                if (count == 100000 || cost > 16U * 1024U * 1024U - queue_bytes) { fprintf(stderr, "rgx: directory queue exceeds safety limit\n"); rgx_had_error = true; free(path); break; }
                queue_bytes += cost;
                tasks = (WalkTask*)rgx_realloc_array(tasks, count + 1, sizeof(WalkTask)); tasks[count++] = (WalkTask){path, task.depth + 1};
            } else { walker_file(walker, path); free(path); }
            if (walker->opts->quiet && rgx_found) break;
        } while (FindNextFileW(find, &data));
        DWORD error = GetLastError(); if (error != ERROR_NO_MORE_FILES && !(walker->opts->quiet && rgx_found)) { fprintf(stderr, "rgx: directory enumeration failed: %s\n", task.path); rgx_had_error = true; }
        FindClose(find); free(task.path);
    }
    while (count) free(tasks[--count].path);
    free(tasks); free(root);
}

void RgxWalker_Free(RgxWalker* walker) {
    if (!walker) return;
    IgnoreRule* rule = walker->rules;
    while (rule) { IgnoreRule* next = rule->next; free(rule->scope); free(rule->pattern); free(rule); rule = next; }
    LoadedScope* scope = walker->loaded;
    while (scope) { LoadedScope* next = scope->next; free(scope->path); free(scope); scope = next; }
    free(walker->git_root);
    free(walker);
}

/* ========================================================================= */
/*                          6. HELP & CLI PARSER                             */
/* ========================================================================= */
