#include "mkfs_cli.h"

#include "mkfs_error.h"

#include <stdint.h>
#include <string.h>
#include <wchar.h>

static bool SetUsageError(MkfsError* error, const wchar_t* message) {
    MkfsErrorSet(error, ERROR_INVALID_PARAMETER, message);
    return false;
}

static bool NextValue(int argc, wchar_t* argv[], int* index, const wchar_t** value,
    MkfsError* error) {
    if (*index + 1 >= argc || argv[*index + 1] == NULL || argv[*index + 1][0] == L'\0') {
        return SetUsageError(error, L"Option requires a value");
    }
    *value = argv[++*index];
    return true;
}

static bool ParseFileSystem(const wchar_t* value, MkfsFileSystem* file_system) {
    if (_wcsicmp(value, L"NTFS") == 0) *file_system = MKFS_FILESYSTEM_NTFS;
    else if (_wcsicmp(value, L"FAT32") == 0) *file_system = MKFS_FILESYSTEM_FAT32;
    else if (_wcsicmp(value, L"exFAT") == 0) *file_system = MKFS_FILESYSTEM_EXFAT;
    else return false;
    return true;
}

static bool ParseDriveTarget(const wchar_t* value, wchar_t* drive_letter) {
    const size_t length = wcslen(value);
    if (length == 0 || length > 3) return false;
    const wchar_t letter = value[0];
    if (!((letter >= L'A' && letter <= L'Z') || (letter >= L'a' && letter <= L'z'))) {
        return false;
    }
    if (length == 2 && value[1] != L':') return false;
    if (length == 3 && (value[1] != L':' || (value[2] != L'\\' && value[2] != L'/'))) return false;
    *drive_letter = letter >= L'a' && letter <= L'z'
        ? (wchar_t)(letter - L'a' + L'A')
        : letter;
    return true;
}

static bool ParseClusterSize(const wchar_t* text, DWORD* cluster_size) {
    static const DWORD supported_sizes[] = {
        512, 1024, 2048, 4096, 8192, 16384, 32768, 65536
    };
    if (text == NULL || *text == L'\0') return false;
    uint64_t value = 0;
    for (const wchar_t* cursor = text; *cursor != L'\0'; ++cursor) {
        if (*cursor < L'0' || *cursor > L'9') return false;
        const uint64_t digit = (uint64_t)(*cursor - L'0');
        if (value > (UINT32_MAX - digit) / 10u) return false;
        value = value * 10u + digit;
    }
    for (size_t i = 0; i < _countof(supported_sizes); ++i) {
        if (value == supported_sizes[i]) {
            *cluster_size = (DWORD)value;
            return true;
        }
    }
    return false;
}

static bool ValidateLabel(const MkfsOptions* options, MkfsError* error) {
    if (!options->has_label) return true;
    size_t maximum_length = 32;
    if (options->file_system == MKFS_FILESYSTEM_FAT32) maximum_length = 11;
    else if (options->file_system == MKFS_FILESYSTEM_EXFAT) maximum_length = 15;
    const size_t length = wcslen(options->label);
    if (length > maximum_length ||
        (length != 0 && (options->label[length - 1] == L' ' ||
                         options->label[length - 1] == L'.'))) {
        return SetUsageError(error, L"Volume label is too long or ends with a space or period");
    }
    for (size_t i = 0; i < length; ++i) {
        const wchar_t character = options->label[i];
        if (character < 0x20 || character == L'\\' || character == L'/' ||
            character == L':' || character == L'*' || character == L'?' ||
            character == L'"' || character == L'<' || character == L'>' ||
            character == L'|') {
            return SetUsageError(error, L"Volume label contains a character not supported by Windows");
        }
    }
    return true;
}

void MkfsPrintHelp(void) {
    wprintf(
        L"Usage: mkfs [options] <drive>\n\n"
        L"Format a mounted Windows volume as NTFS, FAT32, or exFAT.\n\n"
        L"Options:\n"
        L"  -t, --type <type>       File system (NTFS, FAT32, exFAT; default NTFS)\n"
        L"  -L, --label <label>     Set the volume label\n"
        L"  -c, --cluster <bytes>   Cluster size: 512 through 65536, powers of two\n"
        L"  -q, --quick             Quick format (default)\n"
        L"  -f, --full              Full format\n"
        L"  -y, --yes               Skip the confirmation prompt\n"
        L"  -v, --verbose           Show format settings and progress\n"
        L"  -V, --version           Display version information\n"
        L"  -h, --help              Display this help\n\n"
        L"The target must be a drive letter such as E:, E:\\, or E.\n"
        L"Formatting erases all data on the selected volume. Use -y only when\n"
        L"the target has been positively identified.\n\n"
        L"Examples:\n"
        L"  mkfs E:\n"
        L"  mkfs -t FAT32 -L USB_STICK F:\n"
        L"  mkfs -t exFAT -c 65536 -y G:\n"
        L"  mkfs -t NTFS -f D:\n");
}

void MkfsPrintVersion(void) {
    wprintf(L"mkfs 2.1.0\n");
}

bool MkfsOptionsParse(int argc, wchar_t* argv[], MkfsOptions* options, MkfsError* error) {
    if (argc < 1 || argv == NULL || options == NULL) {
        MkfsErrorSet(error, ERROR_INVALID_PARAMETER, L"Invalid command-line arguments");
        return false;
    }
    memset(options, 0, sizeof(*options));
    options->file_system = MKFS_FILESYSTEM_NTFS;
    options->quick_format = true;
    MkfsErrorClear(error);
    if (argc == 1) {
        options->show_help = true;
        return true;
    }

    bool target_seen = false;
    bool formatting_option_seen = false;
    bool full_option_seen = false;
    bool quick_option_seen = false;
    bool cluster_option_seen = false;
    bool label_option_seen = false;
    bool type_option_seen = false;
    bool positional_only = false;
    for (int i = 1; i < argc; ++i) {
        const wchar_t* argument = argv[i];
        if (argument == NULL) return SetUsageError(error, L"Invalid command-line argument");
        if (!positional_only && wcscmp(argument, L"--") == 0) {
            positional_only = true;
            continue;
        }
        if (!positional_only && (wcscmp(argument, L"-h") == 0 ||
                wcscmp(argument, L"--help") == 0)) {
            options->show_help = true;
            continue;
        }
        if (!positional_only && (wcscmp(argument, L"-V") == 0 ||
                wcscmp(argument, L"--version") == 0)) {
            options->show_version = true;
            continue;
        }
        if (!positional_only && (wcscmp(argument, L"-t") == 0 ||
                wcscmp(argument, L"--type") == 0)) {
            const wchar_t* value = NULL;
            if (type_option_seen) return SetUsageError(error, L"Specify the file system only once");
            if (!NextValue(argc, argv, &i, &value, error)) return false;
            if (!ParseFileSystem(value, &options->file_system)) {
                return SetUsageError(error, L"File system must be NTFS, FAT32, or exFAT");
            }
            type_option_seen = true;
            formatting_option_seen = true;
            continue;
        }
        if (!positional_only && (wcscmp(argument, L"-L") == 0 ||
                wcscmp(argument, L"--label") == 0)) {
            const wchar_t* value = NULL;
            if (label_option_seen) return SetUsageError(error, L"Specify the volume label only once");
            if (!NextValue(argc, argv, &i, &value, error)) return false;
            const size_t length = wcslen(value);
            if (length >= _countof(options->label)) {
                return SetUsageError(error, L"Volume label exceeds the maximum supported length");
            }
            if (wcscpy_s(options->label, _countof(options->label), value) != 0) {
                return SetUsageError(error, L"Could not store the volume label");
            }
            options->has_label = true;
            label_option_seen = true;
            formatting_option_seen = true;
            continue;
        }
        if (!positional_only && (wcscmp(argument, L"-c") == 0 ||
                wcscmp(argument, L"--cluster") == 0)) {
            const wchar_t* value = NULL;
            if (cluster_option_seen) return SetUsageError(error, L"Specify cluster size only once");
            if (!NextValue(argc, argv, &i, &value, error)) return false;
            if (!ParseClusterSize(value, &options->cluster_size)) {
                return SetUsageError(error, L"Cluster size must be 512, 1024, 2048, 4096, 8192, 16384, 32768, or 65536 bytes");
            }
            cluster_option_seen = true;
            formatting_option_seen = true;
            continue;
        }
        if (!positional_only && (wcscmp(argument, L"-f") == 0 ||
                wcscmp(argument, L"--full") == 0)) {
            if (full_option_seen || quick_option_seen) {
                return SetUsageError(error, L"Choose either quick or full format");
            }
            options->quick_format = false;
            full_option_seen = true;
            formatting_option_seen = true;
            continue;
        }
        if (!positional_only && (wcscmp(argument, L"-q") == 0 ||
                wcscmp(argument, L"--quick") == 0)) {
            if (quick_option_seen || full_option_seen) {
                return SetUsageError(error, L"Choose either quick or full format");
            }
            options->quick_format = true;
            quick_option_seen = true;
            formatting_option_seen = true;
            continue;
        }
        if (!positional_only && (wcscmp(argument, L"-y") == 0 ||
                wcscmp(argument, L"--yes") == 0)) {
            options->assume_yes = true;
            formatting_option_seen = true;
            continue;
        }
        if (!positional_only && (wcscmp(argument, L"-v") == 0 ||
                wcscmp(argument, L"--verbose") == 0)) {
            options->verbose = true;
            formatting_option_seen = true;
            continue;
        }
        if (!positional_only && argument[0] == L'-') {
            return SetUsageError(error, L"Unknown option");
        }
        if (target_seen) return SetUsageError(error, L"Specify exactly one drive target");
        if (!ParseDriveTarget(argument, &options->drive_letter)) {
            return SetUsageError(error, L"Target must be a drive letter such as E: or E:\\");
        }
        target_seen = true;
    }

    if (options->show_help || options->show_version) {
        if ((options->show_help && options->show_version) ||
            formatting_option_seen || target_seen) {
            return SetUsageError(error, L"Help or version cannot be combined with formatting options");
        }
        return true;
    }
    if (!target_seen) return SetUsageError(error, L"A drive target is required");
    return ValidateLabel(options, error);
}
