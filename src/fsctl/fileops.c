#include "app.h"
#include "fileops.h"
#include "dirlist.h"
#include "format.h"

// --- Copy & Cut to Clipboard Buffer ---
void clipboard_copy_or_cut(bool is_cut) {
    if (g_app.count == 0) return;
    FileEntry *curr = &g_app.entries[g_app.selected];
    if (swprintf_s(g_app.clipboard_path, MAX_PATH, L"%ls\\%ls", g_app.current_path, curr->name) < 0) return;
    g_app.clipboard_is_cut = is_cut;

    wprintf(L"\x1b[%d;1H\x1b[K\x1b[42;30m %ls '%ls' to buffer. Navigate and press 'p' to paste.\x1b[0m",
            g_app.term_height, is_cut ? L"Cut" : L"Copied", curr->name);
    fflush(stdout);
    Sleep(600);
}

// --- Paste from Clipboard Buffer ---
void clipboard_paste(void) {
    if (g_app.readonly_mode) {
        wprintf(L"\x1b[%d;1H\x1b[K\x1b[41;37m Cannot paste: Read-only mode is active.\x1b[0m", g_app.term_height);
        fflush(stdout);
        Sleep(800);
        return;
    }
    if (g_app.clipboard_path[0] == L'\0') {
        wprintf(L"\x1b[%d;1H\x1b[K\x1b[43;30m Clipboard is empty. Press 'y' to copy or 'x' to cut.\x1b[0m", g_app.term_height);
        fflush(stdout);
        Sleep(800);
        return;
    }

    const wchar_t *fname = wcsrchr(g_app.clipboard_path, L'\\');
    fname = fname ? fname + 1 : g_app.clipboard_path;

    wchar_t src[MAX_PATH + 2];
    wchar_t dst[MAX_PATH + 2];
    memset(src, 0, sizeof(src));
    memset(dst, 0, sizeof(dst));

    wcsncpy(src, g_app.clipboard_path, MAX_PATH);
    if (swprintf_s(dst, MAX_PATH, L"%ls\\%ls", g_app.current_path, fname) < 0) return;

    src[wcslen(src) + 1] = L'\0';
    dst[wcslen(dst) + 1] = L'\0';

    SHFILEOPSTRUCTW op;
    memset(&op, 0, sizeof(op));
    op.wFunc = g_app.clipboard_is_cut ? FO_MOVE : FO_COPY;
    op.pFrom = src;
    op.pTo = dst;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMMKDIR;

    int res = SHFileOperationW(&op);
    if (res == 0 && !op.fAnyOperationsAborted) {
        if (g_app.clipboard_is_cut) {
            g_app.clipboard_path[0] = L'\0';
        }
        load_directory();
    } else {
        wprintf(L"\x1b[%d;1H\x1b[K\x1b[41;37m Paste failed (Code 0x%X). Press any key.\x1b[0m", g_app.term_height, res);
        fflush(stdout);
        _getch();
    }
}

// --- File Properties Dialog ---
void show_properties(void) {
    if (g_app.count == 0) return;
    FileEntry *curr = &g_app.entries[g_app.selected];

    wchar_t target[MAX_PATH];
    if (swprintf_s(target, MAX_PATH, L"%ls\\%ls", g_app.current_path, curr->name) < 0) return;

    DWORD attrs = GetFileAttributesW(target);
    if (attrs == INVALID_FILE_ATTRIBUTES) return;

    wchar_t sz[32], dt[32], ct[32], at[32];
    format_size(curr->size, sz, 32);
    format_filetime(curr->mtime, dt, 32);

    HANDLE h = CreateFileW(target, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, curr->is_dir ? FILE_FLAG_BACKUP_SEMANTICS : 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        FILETIME ftC, ftA, ftW;
        if (GetFileTime(h, &ftC, &ftA, &ftW)) {
            format_filetime(ftC, ct, 32);
            format_filetime(ftA, at, 32);
        } else {
            wcscpy(ct, L"N/A"); wcscpy(at, L"N/A");
        }
        CloseHandle(h);
    } else {
        wcscpy(ct, L"N/A"); wcscpy(at, L"N/A");
    }

    const wchar_t *type_str = curr->is_dir ? L"Directory" : L"File";
    wchar_t attr_str[64] = {0};
    if (attrs & FILE_ATTRIBUTE_READONLY)  wcscat(attr_str, L"R ");
    if (attrs & FILE_ATTRIBUTE_HIDDEN)    wcscat(attr_str, L"H ");
    if (attrs & FILE_ATTRIBUTE_SYSTEM)    wcscat(attr_str, L"S ");
    if (attrs & FILE_ATTRIBUTE_ARCHIVE)   wcscat(attr_str, L"A ");
    if (attrs & FILE_ATTRIBUTE_COMPRESSED) wcscat(attr_str, L"C ");
    if (attrs & FILE_ATTRIBUTE_ENCRYPTED)  wcscat(attr_str, L"E ");
    if (attr_str[0] == L'\0') wcscpy(attr_str, L"(none)");

    wchar_t lines[10][80];
    swprintf(lines[0], 80, L"Properties: %ls", curr->name);
    wcscpy(lines[1], L"--------------------------------------------------");
    swprintf(lines[2], 80, L"  Type:       %ls", type_str);
    swprintf(lines[3], 80, L"  Size:       %ls", sz);
    swprintf(lines[4], 80, L"  Modified:   %ls", dt);
    swprintf(lines[5], 80, L"  Created:    %ls", ct);
    swprintf(lines[6], 80, L"  Accessed:   %ls", at);
    swprintf(lines[7], 80, L"  Attributes: %ls", attr_str);
    wcscpy(lines[8], L"--------------------------------------------------");
    wcscpy(lines[9], L"  Press any key to dismiss.");

    int total = 10;
    int box_w = 54;
    int sy = (g_app.term_height - total) / 2;
    int sx = (g_app.term_width - box_w) / 2;

    for (int i = 0; i < total; i++) {
        wprintf(L"\x1b[%d;%dH\x1b[44;37m  %-*ls\x1b[0m", sy + i, sx, box_w - 2, lines[i]);
    }
    fflush(stdout);
    _getch();
}

// --- Rename File ---
void rename_file(void) {
    if (g_app.readonly_mode || g_app.count == 0) return;
    FileEntry *curr = &g_app.entries[g_app.selected];

    wchar_t prompt[MAX_PATH + 32];
    swprintf(prompt, MAX_PATH + 32, L"Rename '%ls' to: ", curr->name);
    int prompt_len = (int)wcslen(prompt);

    wprintf(L"\x1b[%d;1H\x1b[K\x1b[47;30m%ls\x1b[0m", g_app.term_height, prompt);
    fflush(stdout);

    wchar_t new_name[MAX_PATH] = {0};
    int pos = 0;
    while (true) {
        int c = _getch();
        if (c == 27) return;
        if (c == 13) break;
        if (c == 8) {
            if (pos > 0) {
                pos--;
                new_name[pos] = L'\0';
                wprintf(L"\x1b[%d;%dH \x1b[%d;%dH", g_app.term_height, prompt_len + pos + 1,
                        g_app.term_height, prompt_len + pos + 1);
                fflush(stdout);
            }
            continue;
        }
        if (c >= 32 && pos < MAX_PATH - 1) {
            new_name[pos++] = (wchar_t)c;
            new_name[pos] = L'\0';
            wprintf(L"%lc", (wchar_t)c);
            fflush(stdout);
        }
    }

    if (pos == 0 || wcscmp(new_name, curr->name) == 0) return;

    wchar_t old_path[MAX_PATH], new_path[MAX_PATH];
    if (swprintf_s(old_path, MAX_PATH, L"%ls\\%ls", g_app.current_path, curr->name) < 0 ||
        swprintf_s(new_path, MAX_PATH, L"%ls\\%ls", g_app.current_path, new_name) < 0) {
        return;
    }

    if (MoveFileW(old_path, new_path)) {
        load_directory();
    }
}

// --- Safe File/Directory Deletion ---
void delete_item(void) {
    if (g_app.readonly_mode || g_app.count == 0) return;
    FileEntry *curr = &g_app.entries[g_app.selected];

    wchar_t prompt[MAX_PATH + 64];
    swprintf(prompt, MAX_PATH + 64, L"Send %ls '%ls' to Recycle Bin? [y/N]",
             curr->is_dir ? L"folder" : L"file", curr->name);

    wprintf(L"\x1b[%d;1H\x1b[K\x1b[41;37m %ls\x1b[0m", g_app.term_height, prompt);
    fflush(stdout);

    int ch = _getch();
    if (ch != 'y' && ch != 'Y') return;

    wchar_t target[MAX_PATH + 2];
    memset(target, 0, sizeof(target));
    if (swprintf_s(target, MAX_PATH, L"%ls\\%ls", g_app.current_path, curr->name) < 0) return;
    target[wcslen(target) + 1] = L'\0';

    SHFILEOPSTRUCTW op;
    memset(&op, 0, sizeof(op));
    op.wFunc = FO_DELETE;
    op.pFrom = target;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;

    int res = SHFileOperationW(&op);
    if (res == 0 && !op.fAnyOperationsAborted) {
        load_directory();
    } else {
        DWORD err = GetLastError();
        wchar_t err_msg[128];
        if (curr->is_dir) {
            swprintf(err_msg, 128, L"Delete failed (Code 0x%X). Non-empty folder or access denied. Press any key.", res ? res : (int)err);
        } else {
            swprintf(err_msg, 128, L"Delete failed (Code 0x%X). File in use or access denied. Press any key.", res ? res : (int)err);
        }
        wprintf(L"\x1b[%d;1H\x1b[K\x1b[41;37m %ls\x1b[0m", g_app.term_height, err_msg);
        fflush(stdout);
        _getch();
    }
}
