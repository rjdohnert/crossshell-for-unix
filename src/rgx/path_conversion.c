#include "path_conversion.h"
#include "allocation.h"
#include "search_state.h"

wchar_t* path_to_wide(const char* path) {
    int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (!required || required >= MAX_PATH_LEN - 16) { fprintf(stderr, "rgx: invalid or excessive path: %s\n", path); rgx_had_error = true; return NULL; }
    wchar_t* wide = (wchar_t*)rgx_calloc((size_t)required, sizeof(wchar_t));
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, required)) { free(wide); rgx_had_error = true; return NULL; }
    if (wcsncmp(wide, L"\\\\?\\", 4) == 0 || wcsncmp(wide, L"\\\\.\\", 4) == 0) return wide;
    DWORD length = GetFullPathNameW(wide, 0, NULL, NULL);
    if (!length || length >= MAX_PATH_LEN - 16) { fprintf(stderr, "rgx: cannot resolve path: %s\n", path); rgx_had_error = true; free(wide); return NULL; }
    wchar_t* absolute = (wchar_t*)rgx_calloc((size_t)length + 16, sizeof(wchar_t));
    if (!GetFullPathNameW(wide, length, absolute + 8, NULL)) { free(wide); free(absolute); rgx_had_error = true; return NULL; }
    free(wide);
    if (absolute[8] == L'\\' && absolute[9] == L'\\') { memcpy(absolute, L"\\\\?\\UNC\\", 8 * sizeof(wchar_t)); memmove(absolute + 8, absolute + 10, (wcslen(absolute + 10) + 1) * sizeof(wchar_t)); }
    else { memcpy(absolute + 4, L"\\\\?\\", 4 * sizeof(wchar_t)); memmove(absolute, absolute + 4, (wcslen(absolute + 4) + 1) * sizeof(wchar_t)); }
    return absolute;
}

char* wide_to_utf8(const wchar_t* text) {
    int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, NULL, 0, NULL, NULL);
    if (!required) { fprintf(stderr, "rgx: filename contains invalid Unicode\n"); rgx_had_error = true; return NULL; }
    char* result = (char*)rgx_malloc((size_t)required);
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, result, required, NULL, NULL)) { free(result); rgx_had_error = true; return NULL; }
    return result;
}

char* path_join(const char* directory, const char* name) {
    size_t directory_length = strlen(directory), name_length = strlen(name);
    if (directory_length > 131072 || name_length > 131072 || directory_length + name_length > 131072) { fprintf(stderr, "rgx: path length limit exceeded\n"); rgx_had_error = true; return NULL; }
    char* result = (char*)rgx_malloc(directory_length + name_length + 2);
    memcpy(result, directory, directory_length); result[directory_length] = '/'; memcpy(result + directory_length + 1, name, name_length + 1); return result;
}

char* absolute_utf8(const char* path) {
    wchar_t* wide = path_to_wide(path); if (!wide) return NULL;
    wchar_t* plain = wide;
    if (wcsncmp(wide, L"\\\\?\\UNC\\", 8) == 0) { wide[6] = L'\\'; plain = wide + 6; }
    else if (wcsncmp(wide, L"\\\\?\\", 4) == 0) plain = wide + 4;
    char* result = wide_to_utf8(plain); free(wide);
    if (result) { for (char* cursor = result; *cursor; ++cursor) if (*cursor == '\\') *cursor = '/'; size_t length = strlen(result); while (length > 3 && result[length - 1] == '/') result[--length] = 0; }
    return result;
}
