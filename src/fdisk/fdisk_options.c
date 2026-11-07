#include "fdisk_options.h"

#include "fdisk_error.h"

#include <limits.h>
#include <string.h>
#include <wchar.h>

static bool ParseImageSize(const wchar_t* text, uint64_t* size_bytes) {
    uint64_t size_mib = 0;
    if (text == NULL || *text == L'\0') return false;
    for (const wchar_t* cursor = text; *cursor != L'\0'; ++cursor) {
        if (*cursor < L'0' || *cursor > L'9') return false;
        const uint64_t digit = (uint64_t)(*cursor - L'0');
        if (size_mib > (UINT64_MAX / (1024u * 1024u) - digit) / 10u) return false;
        size_mib = size_mib * 10u + digit;
    }
    if (size_mib == 0) return false;
    *size_bytes = size_mib * 1024u * 1024u;
    return true;
}

static bool SetImageAction(
    FdiskOptions* options,
    FdiskImageAction action,
    const wchar_t* path,
    FdiskError* error) {
    if (options->image_action != FDISK_IMAGE_ACTION_NONE || path == NULL ||
        path[0] == L'\0' || path[0] == L'-') {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Specify one image action and a valid image path");
        return false;
    }
    options->image_action = action;
    options->image_path = path;
    return true;
}

static bool RequireOptionValue(int argc, wchar_t* argv[], int* index, const wchar_t** value,
    FdiskError* error) {
    if (*index + 1 >= argc || argv[*index + 1] == NULL || argv[*index + 1][0] == L'\0' ||
        argv[*index + 1][0] == L'-') {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Missing option value");
        return false;
    }
    *value = argv[++*index];
    return true;
}

static bool ParseDiskNumber(const wchar_t* text, uint32_t* number) {
    uint64_t value = 0;
    if (text == NULL || text[0] == L'\0') return false;

    for (const wchar_t* cursor = text; *cursor != L'\0'; ++cursor) {
        if (*cursor < L'0' || *cursor > L'9') return false;
        const uint32_t digit = (uint32_t)(*cursor - L'0');
        if (value > (UINT32_MAX - digit) / 10u) return false;
        value = value * 10u + digit;
    }

    *number = (uint32_t)value;
    return true;
}

static bool ParseDiskTarget(const wchar_t* argument, uint32_t* number) {
    static const wchar_t canonical_prefix[] = L"\\\\.\\PhysicalDrive";
    static const wchar_t short_prefix[] = L"PhysicalDrive";
    const wchar_t* digits = argument;

    if (_wcsnicmp(argument, canonical_prefix, _countof(canonical_prefix) - 1) == 0) {
        digits += _countof(canonical_prefix) - 1;
    } else if (_wcsnicmp(argument, short_prefix, _countof(short_prefix) - 1) == 0) {
        digits += _countof(short_prefix) - 1;
    }

    return ParseDiskNumber(digits, number);
}

bool FdiskParseOptions(int argc, wchar_t* argv[], FdiskOptions* options, FdiskError* error) {
    if (options == NULL || argv == NULL || argc < 1) {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Invalid command-line arguments");
        return false;
    }

    memset(options, 0, sizeof(*options));
    FdiskErrorClear(error);

    if (argc == 1) {
        options->show_help = true;
        return true;
    }

    for (int i = 1; i < argc; ++i) {
        const wchar_t* argument = argv[i];

        if (wcscmp(argument, L"--image") == 0 ||
            wcscmp(argument, L"--create-image") == 0 ||
            wcscmp(argument, L"--resize-image") == 0 ||
            wcscmp(argument, L"--detach-image") == 0) {
            const FdiskImageAction action = wcscmp(argument, L"--image") == 0
                ? FDISK_IMAGE_ACTION_USE
                : wcscmp(argument, L"--create-image") == 0
                    ? FDISK_IMAGE_ACTION_CREATE
                    : wcscmp(argument, L"--resize-image") == 0
                        ? FDISK_IMAGE_ACTION_RESIZE
                        : FDISK_IMAGE_ACTION_DETACH;
            const wchar_t* path = NULL;
            if (!RequireOptionValue(argc, argv, &i, &path, error) ||
                !SetImageAction(options, action, path, error)) return false;
            continue;
        }
        if (wcscmp(argument, L"--size") == 0) {
            const wchar_t* value = NULL;
            if (!RequireOptionValue(argc, argv, &i, &value, error)) return false;
            if (options->has_image_size) {
                FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Specify image size only once");
                return false;
            }
            if (!ParseImageSize(value, &options->image_size_bytes)) {
                FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Image size must be a positive decimal MiB value");
                return false;
            }
            options->has_image_size = true;
            continue;
        }
        if (wcscmp(argument, L"--format") == 0) {
            const wchar_t* value = NULL;
            if (!RequireOptionValue(argc, argv, &i, &value, error)) return false;
            if (options->image_format_explicit) {
                FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Specify the image format only once");
                return false;
            }
            if (_wcsicmp(value, L"vhd") == 0) {
                options->image_format = FDISK_IMAGE_FORMAT_VHD;
            } else if (_wcsicmp(value, L"vhdx") == 0) {
                options->image_format = FDISK_IMAGE_FORMAT_VHDX;
            } else {
                FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Image format must be vhd or vhdx");
                return false;
            }
            options->image_format_explicit = true;
            continue;
        }
        if (wcscmp(argument, L"-h") == 0 || wcscmp(argument, L"--help") == 0) {
            options->show_help = true;
            continue;
        }
        if (wcscmp(argument, L"-V") == 0 || wcscmp(argument, L"--version") == 0) {
            options->show_version = true;
            continue;
        }
        if (wcscmp(argument, L"-l") == 0 || wcscmp(argument, L"--list") == 0) {
            options->list_all = true;
            continue;
        }
        if (wcscmp(argument, L"-u") == 0 || wcscmp(argument, L"--sectors") == 0) {
            options->sector_units = true;
            continue;
        }
        if (argument[0] == L'-') {
            FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Unknown option");
            return false;
        }
        if (options->has_disk) {
            FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Specify at most one disk target");
            return false;
        }
        if (!ParseDiskTarget(argument, &options->disk_number)) {
            FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Invalid disk target; use a decimal index or PhysicalDriveN");
            return false;
        }
        options->has_disk = true;
    }

    if (options->show_help && options->show_version) {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Choose either --help or --version");
        return false;
    }
    if (options->image_action != FDISK_IMAGE_ACTION_NONE) {
        if (options->has_disk) {
            FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Use either a physical disk or a virtual disk image, not both");
            return false;
        }
        if ((options->image_action == FDISK_IMAGE_ACTION_CREATE ||
                options->image_action == FDISK_IMAGE_ACTION_RESIZE) != options->has_image_size) {
            FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"Image creation and resize require --size; other image actions do not use it");
            return false;
        }
        if (options->image_format_explicit &&
            options->image_action != FDISK_IMAGE_ACTION_CREATE) {
            FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"--format is only valid with --create-image");
            return false;
        }
        if (options->list_all && options->image_action != FDISK_IMAGE_ACTION_USE) {
            FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"--list is only valid with --image");
            return false;
        }
        if (options->image_action == FDISK_IMAGE_ACTION_CREATE &&
            !options->image_format_explicit) {
            const wchar_t* extension = wcsrchr(options->image_path, L'.');
            if (extension != NULL && _wcsicmp(extension, L".vhd") == 0) {
                options->image_format = FDISK_IMAGE_FORMAT_VHD;
            }
        }
    } else if (options->has_image_size || options->image_format_explicit) {
        FdiskErrorSet(error, ERROR_INVALID_PARAMETER, L"--size and --format require an image action");
        return false;
    }
    if (!options->show_help && !options->show_version && !options->list_all && !options->has_disk) {
        if (options->image_action == FDISK_IMAGE_ACTION_NONE) options->show_help = true;
    }

    return true;
}
