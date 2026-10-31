#include "app.h"
#include "stream.h"
#include "decompressor.h"
#include "filesystem.h"

struct UncompressApp {
    CliOptions*        options;
    FileSystemManager* fs;
    int                exitCode;
};

UncompressApp* UncompressApp_Create(CliOptions* options) {
    UncompressApp* self = (UncompressApp*)calloc(1, sizeof(UncompressApp));
    if (!self) return NULL;
    self->options = options;
    self->fs = FileSystemManager_Create(options->force, options->keep, options->quiet);
    return self;
}

void UncompressApp_Destroy(UncompressApp* self) {
    if (!self) return;
    FileSystemManager_Destroy(self->fs);
    free(self);
}

static void PrintDecompressionRatio(FILE* stream, uint64_t inBytes, uint64_t outBytes) {
    if (outBytes > 0) {
        double ratio = (1.0 - ((double)inBytes / (double)outBytes)) * 100.0;
        fwprintf(stream, L" -- ratio: %4.1f%%", ratio);
    }
    fwprintf(stream, L"\n");
}

static int UncompressApp_ProcessStream(UncompressApp* self) {
    FileStream* inStream = StdStream_Create(STD_INPUT_HANDLE);
    FileStream* outStream = StdStream_Create(STD_OUTPUT_HANDLE);
    if (!inStream || !outStream) return 1;

    LzwDecompressor* decompressor = LzwDecompressor_Create();
    DecompressResult res = LzwDecompressor_Process(decompressor, &inStream->base, &outStream->base);

    if (res == DECOMP_NOT_COMPRESSED) {
        fwprintf(stderr, L"uncompress: stdin: not in compressed format\n");
    } else if (res == DECOMP_CORRUPT) {
        fwprintf(stderr, L"uncompress: stdin: corrupt input\n");
    }

    LzwDecompressor_Destroy(decompressor);
    inStream->base.vptr->Destroy(&inStream->base);
    outStream->base.vptr->Destroy(&outStream->base);
    return (res == DECOMP_OK) ? 0 : ((res == DECOMP_NOT_COMPRESSED) ? 2 : 1);
}

static int UncompressApp_ProcessFile(UncompressApp* self, const wchar_t* inputSpec) {
    wchar_t resolvedInPath[PATH_BUFFER_CAPACITY];
    wcsncpy_s(resolvedInPath, PATH_BUFFER_CAPACITY, inputSpec, _TRUNCATE);

    DWORD attrs = GetFileAttributesW(LongPath(resolvedInPath));
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        if (!FileSystemManager_HasZSuffix(resolvedInPath)) {
            wcsncat_s(resolvedInPath, PATH_BUFFER_CAPACITY, L".Z", _TRUNCATE);
            attrs = GetFileAttributesW(LongPath(resolvedInPath));
        }
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            fwprintf(stderr, L"uncompress: %ls: No such file or directory\n", inputSpec);
            return 1;
        }
    }

    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        if (!self->options->quiet) {
            fwprintf(stderr, L"uncompress: %ls is a directory -- ignored\n", resolvedInPath);
        }
        return 1;
    }

    if (!self->options->toStdout && !FileSystemManager_HasZSuffix(resolvedInPath)) {
        if (!self->options->quiet) {
            fwprintf(stderr, L"uncompress: %ls: unknown suffix -- ignored\n", resolvedInPath);
        }
        return 1;
    }

    wchar_t targetPath[PATH_BUFFER_CAPACITY];
    wcsncpy_s(targetPath, PATH_BUFFER_CAPACITY, resolvedInPath, _TRUNCATE);
    if (FileSystemManager_HasZSuffix(targetPath)) {
        targetPath[wcslen(targetPath) - 2] = L'\0';
    }

    if (!self->options->toStdout) {
        DWORD targetAttrs = GetFileAttributesW(LongPath(targetPath));
        if (targetAttrs != INVALID_FILE_ATTRIBUTES && !self->options->force) {
            if (!FileSystemManager_PromptOverwrite(targetPath)) {
                if (!self->options->quiet) {
                    fwprintf(stderr, L"\tfile not overwritten: %ls\n", targetPath);
                }
                return 0;
            }
        }
    }

    HANDLE hIn = CreateFileW(LongPath(resolvedInPath), GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hIn == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"uncompress: cannot open: %ls\n", resolvedInPath);
        return 1;
    }

    FileStream* inStream = FileStream_Create(hIn, true);
    IStream* outStream = NULL;
    wchar_t tempPath[PATH_BUFFER_CAPACITY] = { 0 };

    if (self->options->toStdout) {
        outStream = (IStream*)StdStream_Create(STD_OUTPUT_HANDLE);
    } else {
        wchar_t dir[PATH_BUFFER_CAPACITY];
        wcsncpy_s(dir, PATH_BUFFER_CAPACITY, targetPath, _TRUNCATE);
        wchar_t* lastSlash = wcsrchr(dir, L'\\');
        if (!lastSlash) lastSlash = wcsrchr(dir, L'/');
        if (lastSlash) *lastSlash = L'\0'; else wcscpy_s(dir, PATH_BUFFER_CAPACITY, L".");

        swprintf_s(tempPath, PATH_BUFFER_CAPACITY, L"%ls\\uc_%lu_%lu.tmp",
                   dir, GetCurrentProcessId(), GetTickCount());

        wcsncpy_s(g_ActiveTempPath, PATH_BUFFER_CAPACITY, tempPath, _TRUNCATE);

        HANDLE hTemp = CreateFileW(LongPath(tempPath), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
        if (hTemp == INVALID_HANDLE_VALUE) {
            fwprintf(stderr, L"uncompress: cannot create temporary file: %ls\n", tempPath);
            inStream->base.vptr->Destroy(&inStream->base);
            return 1;
        }

        g_ActiveTempHandle = hTemp;
        outStream = (IStream*)FileStream_Create(hTemp, true);
    }

    LzwDecompressor* decompressor = LzwDecompressor_Create();
    DecompressResult res = LzwDecompressor_Process(decompressor, &inStream->base, outStream);

    if (res != DECOMP_OK) {
        if (res == DECOMP_NOT_COMPRESSED) {
            fwprintf(stderr, L"uncompress: %ls: not in compressed format\n", resolvedInPath);
        } else {
            fwprintf(stderr, L"uncompress: %ls: corrupt input\n", resolvedInPath);
        }
        LzwDecompressor_Destroy(decompressor);
        inStream->base.vptr->Destroy(&inStream->base);
        outStream->vptr->Destroy(outStream);
        if (tempPath[0] != L'\0') {
            g_ActiveTempHandle = INVALID_HANDLE_VALUE;
            DeleteFileW(LongPath(tempPath));
            g_ActiveTempPath[0] = L'\0';
        }
        return (res == DECOMP_NOT_COMPRESSED) ? 2 : 1;
    }

    uint64_t bytesIn = decompressor->bytesIn;
    uint64_t bytesOut = decompressor->bytesOut;
    LzwDecompressor_Destroy(decompressor);

    if (!self->options->toStdout) {
        FileSystemManager_PreserveTimestamps(((FileStream*)inStream)->handle,
                                             ((FileStream*)outStream)->handle);
    }

    // Explicitly close streams before replacing files on disk
    inStream->base.vptr->Destroy(&inStream->base);
    outStream->vptr->Destroy(outStream);
    g_ActiveTempHandle = INVALID_HANDLE_VALUE;

    if (!self->options->toStdout) {
        g_ActiveTempPath[0] = L'\0';

        // Atomic replace or fallback to move
        if (!ReplaceFileW(LongPath(targetPath), LongPath(tempPath), NULL, REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL)) {
            if (!MoveFileExW(LongPath(tempPath), LongPath(targetPath), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
                fwprintf(stderr, L"uncompress: cannot write destination file: %ls\n", targetPath);
                DeleteFileW(tempPath);
                return 1;
            }
        }

        if (!self->options->keep) {
            DeleteFileW(LongPath(resolvedInPath));
        }
    }

    if (self->options->verbose) {
        if (self->options->toStdout) {
            fwprintf(stdout, L"%ls: uncompressed to stdout", resolvedInPath);
        } else {
            fwprintf(stdout, L"%ls: -- replaced with %ls", resolvedInPath, targetPath);
        }
        PrintDecompressionRatio(stdout, bytesIn, bytesOut);
    }

    return 0;
}

int UncompressApp_Run(UncompressApp* self) {
    if (self->options->fileCount == 0) {
        return UncompressApp_ProcessStream(self);
    }

    for (int i = 0; i < self->options->fileCount && !g_Interrupted; ++i) {
        const wchar_t* file = self->options->fileList[i];
        int fileStatus = (wcscmp(file, L"-") == 0)
            ? UncompressApp_ProcessStream(self)
            : UncompressApp_ProcessFile(self, file);

        if (fileStatus != 0 && self->exitCode == 0) {
            self->exitCode = fileStatus;
        }
    }
    return self->exitCode;
}
