#include "app.h"
#include "bytestream.h"
#include "codec.h"
#include "metadata.h"
#include "cleanup.h"

CompressApp *CompressApp_create(void) {
    CompressApp *this = (CompressApp *)calloc(1, sizeof(CompressApp));
    if (!this) return NULL;
    this->config.max_bits = DEFAULT_BITS;
    return this;
}

void CompressApp_destroy(CompressApp *this) {
    if (this) {
        free(this->files);
        free(this);
    }
}

static void print_help(void) {
    wprintf(L"Usage: compress [OPTION]... [FILE]...\n");
    wprintf(L"   or: uncompress [OPTION]... [FILE]...\n\n");
    wprintf(L"Compress or uncompress files using LZW encoding fully wire-compatible with\n");
    wprintf(L"classic UNIX .Z archives (ncompress, gzip -d, 7-Zip).\n\n");
    wprintf(L"Options:\n");
    wprintf(L"  -b, --bits BITS      set maximum code width (9..16, default: 16)\n");
    wprintf(L"  -c, --stdout         write output to standard output; leave files unchanged\n");
    wprintf(L"  -d, --decompress     decompress mode (identical to uncompress)\n");
    wprintf(L"  -f, --force          force overwrite and allow compression even if file grows\n");
    wprintf(L"  -k, --keep           keep (do not remove) original files\n");
    wprintf(L"  -v, --verbose        display compression statistics and ratio\n");
    wprintf(L"  -h, --help           display this help information and exit\n");
    wprintf(L"  -V, --version        print version details and exit\n\n");
    wprintf(L"Standard input is processed if no files or '-' are provided.\n");
}

static void print_version(void) {
    wprintf(L"compress %ls\n", COMPRESS_VERSION);
    wprintf(L"Copyright (C) 2026, PC/OpenSystems LLC\n");
}

static bool parse_args(CompressApp *this, int argc, wchar_t **argv) {
    wchar_t *prog = wcsrchr(argv[0], L'\\');
    prog = prog ? (prog + 1) : argv[0];
    if (_wcsicmp(prog, L"uncompress") == 0 || _wcsicmp(prog, L"uncompress.exe") == 0) {
        this->config.decompress = true;
    }

    this->files = (wchar_t **)calloc(argc, sizeof(wchar_t *));
    this->file_count = 0;

    for (int i = 1; i < argc; i++) {
        wchar_t *arg = argv[i];
        if (arg[0] == L'-' && arg[1] != L'\0') {
            if (wcscmp(arg, L"--help") == 0 || wcscmp(arg, L"-h") == 0) {
                print_help();
                exit(0);
            }
            if (wcscmp(arg, L"--version") == 0 || wcscmp(arg, L"-V") == 0) {
                print_version();
                exit(0);
            }
            if (wcscmp(arg, L"--stdout") == 0)     { this->config.to_stdout = true; continue; }
            if (wcscmp(arg, L"--decompress") == 0) { this->config.decompress = true; continue; }
            if (wcscmp(arg, L"--force") == 0)      { this->config.force = true; continue; }
            if (wcscmp(arg, L"--keep") == 0)       { this->config.keep = true; continue; }
            if (wcscmp(arg, L"--verbose") == 0)    { this->config.verbose = true; continue; }

            for (size_t j = 1; j < wcslen(arg); j++) {
                switch (arg[j]) {
                    case L'c': this->config.to_stdout = true; break;
                    case L'd': this->config.decompress = true; break;
                    case L'f': this->config.force = true; break;
                    case L'k': this->config.keep = true; break;
                    case L'v': this->config.verbose = true; break;
                    case L'b':
                        if (arg[j + 1] != L'\0') {
                            this->config.max_bits = _wtoi(&arg[j + 1]);
                            j = wcslen(arg) - 1;
                        } else if (i + 1 < argc) {
                            this->config.max_bits = _wtoi(argv[++i]);
                        } else {
                            fwprintf(stderr, L"compress: missing argument for '-b'\n");
                            return false;
                        }
                        if (this->config.max_bits < 9 || this->config.max_bits > 16) {
                            fwprintf(stderr, L"compress: invalid bits parameter %d (must be 9..16)\n", this->config.max_bits);
                            return false;
                        }
                        break;
                    default:
                        fwprintf(stderr, L"compress: unknown option -- '%c'\n", arg[j]);
                        return false;
                }
            }
        } else {
            this->files[this->file_count++] = arg;
        }
    }
    return true;
}

static bool process_stream(CompressApp *this, IByteStream *in, IByteStream *out) {
    ICodec *codec = this->config.decompress
                    ? LzwDecompressor_create()
                    : LzwCompressor_create(this->config.max_bits);
    if (!codec) return false;

    bool ok = codec->vptr->execute(codec, in, out);
    codec->vptr->destroy(codec);
    return ok;
}

static bool process_file(CompressApp *this, const wchar_t *src_path) {
    wchar_t *actual_src_path = NULL;
    wchar_t *dst_path = NULL;
    wchar_t *tmp_path = NULL;
    size_t path_len = wcslen(src_path);

    if (this->config.decompress) {
        if (path_len > 2 && _wcsicmp(&src_path[path_len - 2], L".Z") == 0) {
            actual_src_path = _wcsdup(src_path);
            dst_path = (wchar_t *)malloc((path_len + 1) * sizeof(wchar_t));
            if (!actual_src_path || !dst_path) goto cleanup_alloc;
            wmemcpy(dst_path, src_path, path_len - 2);
            dst_path[path_len - 2] = L'\0';
        } else {
            wchar_t candidate[32768];
            swprintf(candidate, 32768, L"%ls.Z", src_path);
            DWORD attr = GetFileAttributesW(candidate);
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                actual_src_path = _wcsdup(candidate);
                dst_path = _wcsdup(src_path);
                if (!actual_src_path || !dst_path) goto cleanup_alloc;
            } else {
                fwprintf(stderr, L"compress: %ls: unrecognized extension or %ls not found -- ignored\n", src_path, candidate);
                return false;
            }
        }
    } else {
        actual_src_path = _wcsdup(src_path);
        dst_path = (wchar_t *)malloc((path_len + 16) * sizeof(wchar_t));
        if (!actual_src_path || !dst_path) goto cleanup_alloc;
        swprintf(dst_path, path_len + 16, L"%ls.Z", src_path);
    }

    size_t dst_len = wcslen(dst_path);
    tmp_path = (wchar_t *)malloc((dst_len + 64) * sizeof(wchar_t));
    if (!tmp_path) goto cleanup_alloc;

    DWORD src_attr = GetFileAttributesW(actual_src_path);
    if (src_attr == INVALID_FILE_ATTRIBUTES) {
        fwprintf(stderr, L"compress: %ls: No such file or directory\n", actual_src_path);
        free(actual_src_path);
        free(dst_path);
        free(tmp_path);
        return false;
    }
    if (src_attr & FILE_ATTRIBUTE_DIRECTORY) {
        fwprintf(stderr, L"compress: %ls: Is a directory -- ignored\n", actual_src_path);
        free(actual_src_path);
        free(dst_path);
        free(tmp_path);
        return false;
    }

    if (!this->config.to_stdout && !this->config.force) {
        if (GetFileAttributesW(dst_path) != INVALID_FILE_ATTRIBUTES) {
            fwprintf(stderr, L"compress: %ls already exists; do you wish to overwrite (y or n)? ", dst_path);
            wchar_t ans[16];
            if (fgetws(ans, 16, stdin) == NULL || (ans[0] != L'y' && ans[0] != L'Y')) {
                fwprintf(stderr, L"\tfile not overwritten\n");
                free(actual_src_path);
                free(dst_path);
                free(tmp_path);
                return false;
            }
        }
    }

    FileMetadata meta = {0};
    FileMetadata_capture(&meta, actual_src_path);

    IByteStream *in_stream = FileStream_create(actual_src_path, GENERIC_READ, OPEN_EXISTING);
    if (!in_stream) {
        fwprintf(stderr, L"compress: cannot open input '%ls'\n", actual_src_path);
        free(actual_src_path);
        free(dst_path);
        free(tmp_path);
        return false;
    }

    IByteStream *out_stream = NULL;
    if (this->config.to_stdout) {
        out_stream = StdioStream_create(STD_OUTPUT_HANDLE);
    } else {
        swprintf(tmp_path, dst_len + 64, L"%ls.tmp.%lu_%u", dst_path, GetCurrentProcessId(), rand());
        register_active_temp_file(tmp_path);
        out_stream = FileStream_create(tmp_path, GENERIC_WRITE, CREATE_ALWAYS);
    }

    if (!out_stream) {
        fwprintf(stderr, L"compress: cannot open output destination\n");
        in_stream->vptr->destroy(in_stream);
        register_active_temp_file(NULL);
        free(actual_src_path);
        free(dst_path);
        free(tmp_path);
        return false;
    }

    ICodec *codec = this->config.decompress
                    ? LzwDecompressor_create()
                    : LzwCompressor_create(this->config.max_bits);

    bool ok = codec->vptr->execute(codec, in_stream, out_stream);
    uint64_t bytes_in = codec->vptr->get_bytes_in(codec);
    uint64_t bytes_out = codec->vptr->get_bytes_out(codec);
    codec->vptr->destroy(codec);

    in_stream->vptr->destroy(in_stream);
    out_stream->vptr->destroy(out_stream);

    if (!ok) {
        if (!this->config.to_stdout) DeleteFileW(tmp_path);
        register_active_temp_file(NULL);
        free(actual_src_path);
        free(dst_path);
        free(tmp_path);
        return false;
    }

    if (!this->config.decompress && !this->config.force && !this->config.to_stdout) {
        if (bytes_out >= bytes_in && bytes_in > 0) {
            fwprintf(stderr, L"compress: %ls: file would increase in size; unchanged\n", actual_src_path);
            DeleteFileW(tmp_path);
            register_active_temp_file(NULL);
            free(actual_src_path);
            free(dst_path);
            free(tmp_path);
            return false;
        }
    }

    if (!this->config.to_stdout) {
        if (this->config.force) {
            DWORD dst_attr = GetFileAttributesW(dst_path);
            if (dst_attr != INVALID_FILE_ATTRIBUTES && (dst_attr & FILE_ATTRIBUTE_READONLY)) {
                SetFileAttributesW(dst_path, dst_attr & ~FILE_ATTRIBUTE_READONLY);
            }
        }

        if (!MoveFileExW(tmp_path, dst_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            fwprintf(stderr, L"compress: atomic commit failed: Error %lu\n", GetLastError());
            DeleteFileW(tmp_path);
            register_active_temp_file(NULL);
            free(actual_src_path);
            free(dst_path);
            free(tmp_path);
            return false;
        }
        register_active_temp_file(NULL);

        FileMetadata_apply(&meta, dst_path);
    }

    if (this->config.verbose) {
        if (this->config.decompress) {
            wprintf(L"%ls: -- decompressed (%llu bytes)\n", actual_src_path, bytes_out);
        } else {
            double ratio = (bytes_in > 0) ? (1.0 - ((double)bytes_out / (double)bytes_in)) * 100.0 : 0.0;
            wprintf(L"%ls: -- replaced with %ls (%.2f%% reduction)\n", actual_src_path, dst_path, ratio);
        }
    }

    if (!this->config.to_stdout && !this->config.keep) {
        DWORD cur_src_attr = GetFileAttributesW(actual_src_path);
        if (cur_src_attr != INVALID_FILE_ATTRIBUTES && (cur_src_attr & FILE_ATTRIBUTE_READONLY)) {
            SetFileAttributesW(actual_src_path, cur_src_attr & ~FILE_ATTRIBUTE_READONLY);
        }
        if (!DeleteFileW(actual_src_path)) {
            fwprintf(stderr, L"compress: %ls: cannot unlink: Error %lu\n", actual_src_path, GetLastError());
            free(actual_src_path);
            free(dst_path);
            free(tmp_path);
            return false;
        }
    }

    free(actual_src_path);
    free(dst_path);
    free(tmp_path);
    return true;

cleanup_alloc:
    free(actual_src_path);
    free(dst_path);
    free(tmp_path);
    return false;
}

int CompressApp_run(CompressApp *this, int argc, wchar_t **argv) {
    if (!parse_args(this, argc, argv)) {
        return 1;
    }

    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    if ((this->config.to_stdout || this->file_count == 0) && !this->config.decompress && !this->config.force) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (GetFileType(hOut) == FILE_TYPE_CHAR) {
            fwprintf(stderr, L"compress: compressed data not written to a terminal. Use -f to force.\n");
            return 1;
        }
    }

    if (this->file_count == 0) {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        if (GetFileType(hIn) == FILE_TYPE_CHAR) {
            fwprintf(stderr, L"compress: standard input is a terminal -- ignoring\n");
            fwprintf(stderr, L"Try 'compress --help' for more information.\n");
            return 1;
        }

        IByteStream *in_stream = StdioStream_create(STD_INPUT_HANDLE);
        IByteStream *out_stream = StdioStream_create(STD_OUTPUT_HANDLE);
        bool ok = process_stream(this, in_stream, out_stream);
        in_stream->vptr->destroy(in_stream);
        out_stream->vptr->destroy(out_stream);
        return ok ? 0 : 1;
    }

    int exit_code = 0;
    for (int i = 0; i < this->file_count; i++) {
        if (wcscmp(this->files[i], L"-") == 0) {
            HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
            if (GetFileType(hIn) == FILE_TYPE_CHAR) {
                fwprintf(stderr, L"compress: standard input is a terminal -- ignoring\n");
                exit_code = 1;
                continue;
            }

            IByteStream *in_stream = StdioStream_create(STD_INPUT_HANDLE);
            IByteStream *out_stream = StdioStream_create(STD_OUTPUT_HANDLE);
            if (!process_stream(this, in_stream, out_stream)) {
                exit_code = 1;
            }
            in_stream->vptr->destroy(in_stream);
            out_stream->vptr->destroy(out_stream);
        } else {
            if (!process_file(this, this->files[i])) {
                exit_code = 1;
            }
        }
    }

    return exit_code;
}
