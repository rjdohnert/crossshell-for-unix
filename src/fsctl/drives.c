#include "app.h"
#include <ctype.h>
#include "drives.h"
#include "dirlist.h"
#include "format.h"

// --- Network Drive Support ---
void show_network_drives(void) {
    wchar_t lines[30][80];
    int count = 0;

    wcscpy(lines[count++], L"Network Drives");
    wcscpy(lines[count++], L"--------------------------------------------------");

    for (wchar_t drive = L'A'; drive <= L'Z'; drive++) {
        wchar_t letter[3] = { drive, L':', L'\0' };
        wchar_t remote[MAX_PATH] = {0};
        DWORD remote_len = MAX_PATH;

        if (WNetGetConnectionW(letter, remote, &remote_len) == NO_ERROR) {
            swprintf(lines[count], 80, L"  %ls  ->  %ls", letter, remote);
            count++;
            if (count >= 28) break;
        }
    }

    if (count == 2) {
        wcscpy(lines[count++], L"  (no mapped network drives)");
    }

    wcscpy(lines[count++], L"--------------------------------------------------");
    wcscpy(lines[count++], L"  Press any key to dismiss.");

    int box_w = 54;
    int sy = (g_app.term_height - count) / 2;
    int sx = (g_app.term_width - box_w) / 2;

    for (int i = 0; i < count; i++) {
        wprintf(L"\x1b[%d;%dH\x1b[44;37m  %-*ls\x1b[0m", sy + i, sx, box_w - 2, lines[i]);
    }
    fflush(stdout);
    _getch();
}

// --- Drive Selection ---
void show_drive_selector(void) {
    wchar_t lines[30][80];
    int count = 0;
    wchar_t drives[26];
    int drive_count = 0;

    wcscpy(lines[count++], L"Select Drive");
    wcscpy(lines[count++], L"--------------------------------------------------");

    DWORD mask = GetLogicalDrives();
    for (wchar_t d = L'A'; d <= L'Z'; d++) {
        if (mask & (1 << (d - L'A'))) {
            wchar_t root[4] = { d, L':', L'\\', L'\0' };
            UINT type = GetDriveTypeW(root);
            const wchar_t *type_str;
            switch (type) {
                case DRIVE_FIXED:     type_str = L"Fixed";     break;
                case DRIVE_REMOTE:    type_str = L"Network";   break;
                case DRIVE_REMOVABLE: type_str = L"Removable"; break;
                case DRIVE_CDROM:     type_str = L"CD-ROM";    break;
                case DRIVE_RAMDISK:   type_str = L"RAM Disk";  break;
                default:              type_str = L"Unknown";   break;
            }

            wchar_t label[MAX_PATH] = {0};
            GetVolumeInformationW(root, label, MAX_PATH, NULL, NULL, NULL, NULL, 0);

            ULARGE_INTEGER free_bytes, total_bytes;
            wchar_t size_str[32] = L"";
            if (GetDiskFreeSpaceExW(root, &free_bytes, &total_bytes, NULL)) {
                wchar_t free_fmt[16], total_fmt[16];
                format_size(free_bytes.QuadPart, free_fmt, 16);
                format_size(total_bytes.QuadPart, total_fmt, 16);
                swprintf(size_str, 32, L" %ls/%ls free", free_fmt, total_fmt);
            }

            swprintf(lines[count], 80, L"  %c:  %-10ls %-12ls%ls", d, label[0] ? label : L"(no label)", type_str, size_str);
            drives[drive_count++] = d;
            count++;
            if (count >= 28) break;
        }
    }

    wcscpy(lines[count++], L"--------------------------------------------------");
    wcscpy(lines[count++], L"  Press drive letter to switch, Esc to cancel.");

    int box_w = 64;
    int sy = (g_app.term_height - count) / 2;
    int sx = (g_app.term_width - box_w) / 2;

    for (int i = 0; i < count; i++) {
        wprintf(L"\x1b[%d;%dH\x1b[44;37m  %-*ls\x1b[0m", sy + i, sx, box_w - 2, lines[i]);
    }
    fflush(stdout);

    int ch = _getch();
    if (ch == 27) return;

    wchar_t sel = (wchar_t)toupper((wchar_t)ch);
    for (int i = 0; i < drive_count; i++) {
        if (drives[i] == sel) {
            swprintf(g_app.current_path, MAX_PATH, L"%c:\\", sel);
            load_directory();
            return;
        }
    }
}
