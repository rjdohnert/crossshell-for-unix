#include "app.h"
#include "cli.h"

// --- CLI Argument Parsing & Help ---
void print_cli_help(const wchar_t *prog_name) {
    wprintf(
        L"fsctl - File System Control for Windows (v%hs)\n\n"
        L"USAGE:\n"
        L"  %ls [QUALIFIERS]\n\n"
        L"QUALIFIERS:\n"
        L"  -p, --path <DIR>          Specify starting directory [Default: .]\n"
        L"  -a, --all                 Display hidden and system files\n"
        L"  -s, --sort <MODE>         Sort mode: 'name', 'size', or 'date' [Default: name]\n"
        L"  -r, --reverse             Reverse the listing order\n"
        L"  -R, --readonly            Enable read-only mode (prevent file deletion)\n"
        L"  -h, --help                Show this command line help and exit\n"
        L"  -v, --version             Display version information\n\n"
        L"INTERACTIVE KEYBINDINGS:\n"
        L"  [Up/Down] or [k/j]        Navigate selection\n"
        L"  [Enter]   or [l]          Open directory or file\n"
        L"  [Backspace] or [h]        Navigate to parent directory\n"
        L"  [/]                       Interactive search / filter (Esc clears)\n"
        L"  [y]                       Copy (Yank) selected file/directory\n"
        L"  [x]                       Cut selected file/directory\n"
        L"  [p]                       Paste copied or cut item into current directory\n"
        L"  [i]                       Show file properties dialog\n"
        L"  [a]                       Toggle hidden/system files\n"
        L"  [s]                       Cycle sort order\n"
        L"  [r]                       Toggle sort order direction\n"
        L"  [d]                       Delete selected item (Prompts; moves to Recycle Bin)\n"
        L"  [R]                       Rename selected file/directory\n"
        L"  [n]                       List mapped network drives\n"
        L"  [v]                       Switch drive (volume selector)\n"
        L"  [S]                       Launch SSH in new terminal\n"
        L"  [?] or [F1]               Show help modal\n"
        L"  [q] or [ESC]              Exit fsctl\n",
        FSCTL_VERSION, prog_name
    );
}

int cli_parse_args(int argc, wchar_t *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"-h") == 0 || wcscmp(argv[i], L"--help") == 0) {
            print_cli_help(argv[0]);
            return 0;
        } else if (wcscmp(argv[i], L"-v") == 0 || wcscmp(argv[i], L"--version") == 0) {
            wprintf(L"fsctl version %hs\n", FSCTL_VERSION);
            return 0;
        } else if (wcscmp(argv[i], L"-a") == 0 || wcscmp(argv[i], L"--all") == 0) {
            g_app.show_hidden = true;
        } else if (wcscmp(argv[i], L"-r") == 0 || wcscmp(argv[i], L"--reverse") == 0) {
            g_app.reverse_sort = true;
        } else if (wcscmp(argv[i], L"-R") == 0 || wcscmp(argv[i], L"--readonly") == 0) {
            g_app.readonly_mode = true;
        } else if ((wcscmp(argv[i], L"-p") == 0 || wcscmp(argv[i], L"--path") == 0) && i + 1 < argc) {
            GetFullPathNameW(argv[++i], MAX_PATH, g_app.current_path, NULL);
        } else if ((wcscmp(argv[i], L"-s") == 0 || wcscmp(argv[i], L"--sort") == 0) && i + 1 < argc) {
            i++;
            if (_wcsicmp(argv[i], L"size") == 0) g_app.sort_mode = SORT_SIZE;
            else if (_wcsicmp(argv[i], L"date") == 0) g_app.sort_mode = SORT_DATE;
            else g_app.sort_mode = SORT_NAME;
        } else {
            wprintf(L"Unknown qualifier: %ls\nTry '%ls --help' for options.\n", argv[i], argv[0]);
            return 1;
        }
    }
    return -1;
}
