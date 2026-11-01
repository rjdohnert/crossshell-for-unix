#include "app.h"
#include "input.h"
#include "console.h"
#include "dirlist.h"
#include "drives.h"
#include "fileops.h"
#include "filter.h"
#include "ssh.h"

// --- Input Handling ---
void handle_input(void) {
    int ch = _getch();

    if (ch == 0 || ch == 224) {
        int ext = _getch();
        if (g_app.show_help) {
            g_app.show_help = false;
            wprintf(L"\x1b[2J\x1b[H");
            fflush(stdout);
            return;
        }

        switch (ext) {
            case 72: // UP
                if (g_app.selected > 0) g_app.selected--;
                break;
            case 80: // DOWN
                if (g_app.selected < g_app.count - 1) g_app.selected++;
                break;
            case 71: // HOME
                g_app.selected = 0;
                break;
            case 79: // END
                if (g_app.count > 0) g_app.selected = g_app.count - 1;
                break;
            case 59: // F1
                g_app.show_help = !g_app.show_help;
                break;
        }
        return;
    }

    if (g_app.show_help) {
        g_app.show_help = false;
        wprintf(L"\x1b[2J\x1b[H");
        fflush(stdout);
        return;
    }

    switch (ch) {
        case 'q':
        case 27: // ESC
            // If filter is active, clear filter first
            if (g_app.filter[0] != L'\0') {
                g_app.filter[0] = L'\0';
                load_directory();
                break;
            }
            restore_console();
            if (g_app.entries) free(g_app.entries);
            exit(0);
            break;

        case '?':
            g_app.show_help = true;
            break;

        case '/': // Interactive Search / Filter
            prompt_search_filter();
            break;

        case 'y': // Copy (Yank)
            clipboard_copy_or_cut(false);
            break;

        case 'x': // Cut
            clipboard_copy_or_cut(true);
            break;

        case 'p': // Paste
            clipboard_paste();
            break;

        case 'i': // File Properties / Info
            show_properties();
            break;

        case 'k':
            if (g_app.selected > 0) g_app.selected--;
            break;

        case 'j':
            if (g_app.selected < g_app.count - 1) g_app.selected++;
            break;

        case 'a': // Toggle Hidden
            g_app.show_hidden = !g_app.show_hidden;
            load_directory();
            break;

        case 's': // Cycle Sort
            g_app.sort_mode = (g_app.sort_mode + 1) % 3;
            sort_entries();
            break;

        case 'r': // Invert Sort Order
            g_app.reverse_sort = !g_app.reverse_sort;
            sort_entries();
            break;

        case 8:   // Backspace (Go to parent)
        case 'h': {
            wchar_t *last_slash = wcsrchr(g_app.current_path, L'\\');
            if (last_slash && last_slash != g_app.current_path + 2) {
                *last_slash = L'\0';
                load_directory();
            } else if (last_slash && last_slash == g_app.current_path + 2) {
                *(last_slash + 1) = L'\0';
                load_directory();
            }
            break;
        }

        case 13:  // Enter
        case 'l': {
            if (g_app.count == 0) break;
            FileEntry *curr = &g_app.entries[g_app.selected];
            if (curr->is_dir) {
                if (wcscmp(curr->name, L"..") == 0) {
                    wchar_t *last_slash = wcsrchr(g_app.current_path, L'\\');
                    if (last_slash) *last_slash = L'\0';
                } else {
                    size_t cur_len = wcslen(g_app.current_path);
                    size_t name_len = wcslen(curr->name);
                    if (cur_len + 1 + name_len < MAX_PATH) {
                        if (g_app.current_path[cur_len - 1] != L'\\') {
                            g_app.current_path[cur_len] = L'\\';
                            g_app.current_path[cur_len + 1] = L'\0';
                        }
                        wcscat_s(g_app.current_path, MAX_PATH, curr->name);
                    }
                }
                load_directory();
            } else {
                wchar_t target[MAX_PATH];
                if (swprintf_s(target, MAX_PATH, L"%ls\\%ls", g_app.current_path, curr->name) >= 0) {
                    ShellExecuteW(NULL, L"open", target, NULL, NULL, SW_SHOWNORMAL);
                }
            }
            break;
        }

        case 'd': // Delete item
            delete_item();
            break;

        case 'n': // Network drives
            show_network_drives();
            break;

        case 'v': // Drive selection
            show_drive_selector();
            break;

        case 'S': // SSH
            ssh_connect();
            break;

        case 'R': // Rename
            rename_file();
            break;
    }
}
