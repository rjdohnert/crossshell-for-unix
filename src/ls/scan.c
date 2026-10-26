#include "ls.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

static const LsConfig *active_config;

static int DirectoryScanner_CompareNames(const wchar_t *left, const wchar_t *right) {
    while (*left && *right) {
        if (active_config->sort_mode == LS_SORT_VERSION &&
            *left >= L'0' && *left <= L'9' && *right >= L'0' && *right <= L'9') {
            const wchar_t *left_digits = left;
            const wchar_t *right_digits = right;
            while (*left_digits == L'0') left_digits++;
            while (*right_digits == L'0') right_digits++;
            const wchar_t *left_end = left_digits;
            const wchar_t *right_end = right_digits;
            while (*left_end >= L'0' && *left_end <= L'9') left_end++;
            while (*right_end >= L'0' && *right_end <= L'9') right_end++;
            size_t left_length = (size_t)(left_end - left_digits);
            size_t right_length = (size_t)(right_end - right_digits);
            if (left_length != right_length) return left_length < right_length ? -1 : 1;
            int numeric_result = wcsncmp(left_digits, right_digits, left_length);
            if (numeric_result != 0) return numeric_result;
            const wchar_t *left_run = left;
            const wchar_t *right_run = right;
            while (*left_run >= L'0' && *left_run <= L'9') left_run++;
            while (*right_run >= L'0' && *right_run <= L'9') right_run++;
            if (left_run - left != right_run - right) {
                return (left_run - left) < (right_run - right) ? -1 : 1;
            }
            left = left_run;
            right = right_run;
            continue;
        }
        wchar_t left_character = (wchar_t)towlower(*left);
        wchar_t right_character = (wchar_t)towlower(*right);
        if (left_character != right_character) return left_character < right_character ? -1 : 1;
        left++;
        right++;
    }
    if (*left || *right) return *left ? 1 : -1;
    return 0;
}

static const wchar_t *DirectoryScanner_GetExtension(const wchar_t *name) {
    const wchar_t *extension = wcsrchr(name, L'.');
    return extension ? extension + 1 : L"";
}
static bool DirectoryScanner_Add(IDirectoryScanner *self, const IFileRecord *record) {
    if (self->count >= self->capacity) {
        size_t capacity = (self->capacity == 0) ? 64 : self->capacity * 2;
        if (capacity < self->capacity || capacity > SIZE_MAX / sizeof(IFileRecord)) return false;
        IFileRecord *records = (IFileRecord *)realloc(self->records,
                                                      capacity * sizeof(IFileRecord));
        if (!records) return false;
        self->records = records;
        self->capacity = capacity;
    }
    self->records[self->count++] = *record;
    return true;
}

static int DirectoryScanner_Compare(const void *left, const void *right) {
    const IFileRecord *a = (const IFileRecord *)left;
    const IFileRecord *b = (const IFileRecord *)right;
    int result = 0;
    bool a_is_directory = (a->attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    bool b_is_directory = (b->attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (active_config->group_directories_first && a_is_directory != b_is_directory) {
        return a_is_directory ? -1 : 1;
    }
    if (active_config->group_directories_last && a_is_directory != b_is_directory) {
        return a_is_directory ? 1 : -1;
    }
    if (active_config->sort_mode == LS_SORT_TIME) {
        time_t a_time = a->GetSelectedTime(a, active_config);
        time_t b_time = b->GetSelectedTime(b, active_config);
        if (a_time < b_time) result = 1;
        else if (a_time > b_time) result = -1;
    } else if (active_config->sort_mode == LS_SORT_SIZE) {
        if (a->file_size.QuadPart < b->file_size.QuadPart) result = 1;
        else if (a->file_size.QuadPart > b->file_size.QuadPart) result = -1;
    } else if (active_config->sort_mode == LS_SORT_EXTENSION) {
        result = _wcsicmp(DirectoryScanner_GetExtension(a->file_name),
                          DirectoryScanner_GetExtension(b->file_name));
    }
    if (result == 0) result = DirectoryScanner_CompareNames(a->file_name, b->file_name);
    if (result == 0) result = wcscmp(a->file_name, b->file_name);
    return active_config->reverse_sort ? -result : result;
}

static void DirectoryScanner_Sort(IDirectoryScanner *self, const LsConfig *cfg) {
    if (self->count <= 1 || cfg->disable_sort) return;
    active_config = cfg;
    qsort(self->records, self->count, sizeof(IFileRecord), DirectoryScanner_Compare);
}

static void DirectoryScanner_Clear(IDirectoryScanner *self) {
    for (size_t i = 0; i < self->count; i++) {
        self->records[i].Destroy(&self->records[i]);
    }
    free(self->records);
    self->records = NULL;
    self->count = 0;
    self->capacity = 0;
}

static void DirectoryScanner_Destroy(IDirectoryScanner *self) {
    self->Clear(self);
}

IDirectoryScanner DirectoryScanner_Create(void) {
    IDirectoryScanner scanner;
    memset(&scanner, 0, sizeof(scanner));
    scanner.Add = DirectoryScanner_Add;
    scanner.Sort = DirectoryScanner_Sort;
    scanner.Clear = DirectoryScanner_Clear;
    scanner.Destroy = DirectoryScanner_Destroy;
    return scanner;
}