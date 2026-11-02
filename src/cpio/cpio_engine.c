#include "cpio_engine.h"
#include "cpio_path.h"
#include "cpio_inode.h"
#include "cpio_format.h"
#include "cpio_stream.h"
#include "cpio_security.h"

/* ========================================================================= */
/* MODE: CREATE (-o)                                                         */
/* ========================================================================= */

static bool WriteEntry(FILE* out, const char* path, const struct _stat64* st,
                       ArchiveFormat fmt, unsigned long long* totalBytes) {
    char* normPath = _strdup(path);
    if (!normPath) return false;
    NormalizeSlashes(normPath);

    char* entryName = normPath;
    if (entryName[0] == '.' && entryName[1] == '/') {
        entryName += 2;
    }

    uint64_t rawSize = S_ISDIR(st->st_mode) ? 0 : (uint64_t)st->st_size;
    if (rawSize > SVR4_MAX_FILE_SIZE) {
        fprintf(stderr, "cpio: error: '%s' exceeds 4GB format limit (%llu bytes)\n", path, (unsigned long long)rawSize);
        SetWarning();
        free(normPath);
        return false;
    }

    size_t nameSize = strlen(entryName) + 1;
    uint32_t fileSize = (uint32_t)rawSize;
    uint32_t posixMode = SynthesizePosixMode(st);

    /* Format 1: SVR4 newc or CRC */
    if (fmt == FMT_NEWC || fmt == FMT_CRC) {
        const char* magic = (fmt == FMT_CRC) ? "070702" : "070701";
        char header[CPIO_HEADER_SIZE + 1];
        sprintf(header,
            "%-6.6s%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X",
            magic,
            (uint32_t)st->st_ino,
            posixMode,
            (uint32_t)st->st_uid,
            (uint32_t)st->st_gid,
            (uint32_t)st->st_nlink,
            (uint32_t)st->st_mtime,
            fileSize,
            0, 0, 0, 0,
            (uint32_t)nameSize,
            0
        );

        if (!WriteAll(out, header, CPIO_HEADER_SIZE) ||
            !WriteAll(out, entryName, nameSize) ||
            !WriteZeroPadding(out, CalcPadding4(CPIO_HEADER_SIZE + nameSize))) {
            free(normPath);
            return false;
        }
    }
    /* Format 2: POSIX.1 ODC format */
    else {
        char header[77];
        sprintf(header,
            "070707%06o%06o%06o%06o%06o%06o%06o%011o%06o%011o",
            0,
            (uint32_t)st->st_ino & 0777777,
            posixMode & 0777777,
            (uint32_t)st->st_uid & 0777777,
            (uint32_t)st->st_gid & 0777777,
            (uint32_t)st->st_nlink & 0777777,
            0,
            (uint32_t)st->st_mtime,
            (uint32_t)nameSize & 0777777,
            fileSize
        );
        if (!WriteAll(out, header, 76) || !WriteAll(out, entryName, nameSize)) {
            free(normPath);
            return false;
        }
    }

    free(normPath);

    if (fileSize > 0) {
        FILE* in = FileOpenUtf8(path, "rb");
        if (!in) {
            fprintf(stderr, "cpio: warning: failed to open source '%s'\n", path);
            SetWarning();
            char padBlock[BUFFER_CHUNK_SIZE] = {0};
            size_t remaining = fileSize;
            while (remaining > 0) {
                size_t chunk = remaining < sizeof(padBlock) ? remaining : sizeof(padBlock);
                if (!WriteAll(out, padBlock, chunk)) return false;
                remaining -= chunk;
            }
            if (fmt == FMT_NEWC || fmt == FMT_CRC) {
                if (!WriteZeroPadding(out, CalcPadding4(fileSize))) return false;
            }
            *totalBytes += (CPIO_HEADER_SIZE + nameSize + CalcPadding4(CPIO_HEADER_SIZE + nameSize) + fileSize + CalcPadding4(fileSize));
            return true;
        }

        char buffer[BUFFER_CHUNK_SIZE];
        size_t remaining = fileSize;
        while (remaining > 0) {
            if (g_AbortRequested) { fclose(in); return false; }
            size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
            size_t readCount = fread(buffer, 1, chunk, in);
            if (readCount == 0) {
                fprintf(stderr, "cpio: warning: premature EOF on '%s'\n", path);
                SetWarning();
                break;
            }
            if (!WriteAll(out, buffer, readCount)) {
                fclose(in);
                return false;
            }
            remaining -= readCount;
        }
        fclose(in);

        while (remaining > 0) {
            char zero = 0;
            if (!WriteAll(out, &zero, 1)) return false;
            remaining--;
        }

        if (fmt == FMT_NEWC || fmt == FMT_CRC) {
            if (!WriteZeroPadding(out, CalcPadding4(fileSize))) return false;
        }
    }

    size_t writtenHeader = (fmt == FMT_ODC) ? 76 : CPIO_HEADER_SIZE;
    size_t namePad = (fmt == FMT_ODC) ? 0 : CalcPadding4(writtenHeader + nameSize);
    size_t dataPad = (fmt == FMT_ODC) ? 0 : CalcPadding4(fileSize);
    *totalBytes += (writtenHeader + nameSize + namePad + fileSize + dataPad);
    return true;
}

int ExecuteCreate(const CpioConfig* cfg) {
    FILE* out = stdout;
    if (cfg->archivePath) {
        out = FileOpenUtf8(cfg->archivePath, "wb");
        if (!out) {
            fprintf(stderr, "cpio: fatal: cannot open output archive '%s'\n", cfg->archivePath);
            return 2;
        }
    } else {
        ForceBinaryStream(stdout);
    }

    unsigned long long totalBytes = 0;
    int delim = cfg->nullTerminated ? '\0' : '\n';
    char* line = NULL;
    ArchiveFormat fmt = (cfg->createFormat != FMT_UNKNOWN) ? cfg->createFormat : FMT_NEWC;

    while (!g_AbortRequested && (line = ReadStreamLine(stdin, delim)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }

        struct _stat64 st;
        if (Stat64Utf8(line, &st) != 0) {
            fprintf(stderr, "cpio: cannot stat '%s'\n", line);
            SetWarning();
            free(line);
            continue;
        }

        if (WriteEntry(out, line, &st, fmt, &totalBytes)) {
            if (cfg->verbose) printf("%s\n", line);
        } else if (ferror(out)) {
            fprintf(stderr, "cpio: fatal: disk full or write failure on archive\n");
            SetFatal();
            free(line);
            if (out != stdout) fclose(out);
            return 2;
        }
        free(line);
    }

    if (g_AbortRequested) {
        SetFatal();
        if (out != stdout) fclose(out);
        return 2;
    }

    struct _stat64 trailerStat = {0};
    trailerStat.st_nlink = 1;
    if (!WriteEntry(out, CPIO_TRAILER, &trailerStat, fmt, &totalBytes) || ferror(out)) {
        fprintf(stderr, "cpio: fatal: failure writing archive trailer\n");
        SetFatal();
        if (out != stdout) fclose(out);
        return 2;
    }

    /* Terminal block padding */
    unsigned long long blockSize = cfg->blockFactor5120 ? 5120 : 512;
    size_t tailPadding = (size_t)((blockSize - (totalBytes % blockSize)) % blockSize);
    if (tailPadding > 0) {
        if (!WriteZeroPadding(out, tailPadding)) {
            fprintf(stderr, "cpio: fatal: failure writing terminal block padding\n");
            SetFatal();
            if (out != stdout) fclose(out);
            return 2;
        }
        totalBytes += tailPadding;
    }

    if (out != stdout) fclose(out);

    unsigned long long blocks = totalBytes / blockSize;
    fprintf(stderr, "%llu blocks\n", blocks);
    return g_ExitStatus;
}

/* ========================================================================= */
/* MODE: EXTRACT (-i) / LIST (-t)                                            */
/* ========================================================================= */

int ExecuteExtractOrList(const CpioConfig* cfg) {
    FILE* in = stdin;
    if (cfg->archivePath) {
        in = FileOpenUtf8(cfg->archivePath, "rb");
        if (!in) {
            fprintf(stderr, "cpio: fatal: cannot open archive '%s'\n", cfg->archivePath);
            return 2;
        }
    } else {
        ForceBinaryStream(stdin);
    }

    unsigned long long totalBytes = 0;
    bool foundTrailer = false;
    DirTimeNode* dirTimeStack = NULL;
    InodeTable* inodeTable = InodeTable_Create();
    GenericCpioEntry e;

    while (!g_AbortRequested && ReadCpioHeader(in, &e)) {
        if (e.nameSize == 0 || e.nameSize > MAX_PATH_EXTENDED) {
            fprintf(stderr, "cpio: fatal: corrupt header filename length: %u\n", e.nameSize);
            SetFatal();
            break;
        }

        char* rawName = (char*)malloc(e.nameSize + 1);
        if (!rawName) { SetFatal(); break; }

        if (fread(rawName, 1, e.nameSize, in) != e.nameSize) {
            fprintf(stderr, "cpio: fatal: premature EOF reading entry name\n");
            free(rawName);
            SetFatal();
            break;
        }
        rawName[e.nameSize] = '\0';

        if (!StreamSkip(in, e.namePad)) {
            fprintf(stderr, "cpio: fatal: premature EOF skipping filename padding\n");
            free(rawName);
            SetFatal();
            break;
        }

        if (strcmp(rawName, CPIO_TRAILER) == 0) {
            free(rawName);
            foundTrailer = true;
            break;
        }

        if (!ShouldProcessFile(rawName, cfg)) {
            if (!StreamSkip(in, e.fileSize + e.dataPad)) {
                fprintf(stderr, "cpio: fatal: premature EOF skipping filtered entry\n");
                free(rawName);
                SetFatal();
                break;
            }
            totalBytes += (e.fileSize + e.dataPad);
            free(rawName);
            continue;
        }

        /* ---------------- LIST MODE (-t) ---------------- */
        if (cfg->mode == MODE_LIST) {
            if (cfg->verbose) {
                char perm[11] = "----------";
                if (S_ISDIR(e.mode)) perm[0] = 'd';
                else if (S_ISLNK(e.mode)) perm[0] = 'l';
                if (e.mode & 0400) perm[1] = 'r';
                if (e.mode & 0200) perm[2] = 'w';
                if (e.mode & 0100) perm[3] = 'x';
                if (e.mode & 0040) perm[4] = 'r';
                if (e.mode & 0020) perm[5] = 'w';
                if (e.mode & 0010) perm[6] = 'x';
                if (e.mode & 0004) perm[7] = 'r';
                if (e.mode & 0002) perm[8] = 'w';
                if (e.mode & 0001) perm[9] = 'x';

                char timeBuf[32];
                time_t t = (time_t)e.mtime;
                struct tm* tmInfo = localtime(&t);
                if (tmInfo) {
                    strftime(timeBuf, sizeof(timeBuf), "%b %d %H:%M:%S %Y", tmInfo);
                } else {
                    strcpy(timeBuf, "Unknown Time");
                }
                printf("%s %8llu %s %s\n", perm, (unsigned long long)e.fileSize, timeBuf, rawName);
            } else {
                printf("%s\n", rawName);
            }
            if (!StreamSkip(in, e.fileSize + e.dataPad)) {
                fprintf(stderr, "cpio: fatal: premature EOF skipping listed entry\n");
                free(rawName);
                SetFatal();
                break;
            }
        }
        /* ---------------- EXTRACT MODE (-i) ---------------- */
        else {
            char* targetPath = NULL;
            if (!SanitizePath(rawName, &targetPath)) {
                SetWarning();
                StreamSkip(in, e.fileSize + e.dataPad);
                free(rawName);
                continue;
            }

            if (cfg->makeDirs) {
                char* dirCopy = _strdup(targetPath);
                if (dirCopy) {
                    char* lastSlash = strrchr(dirCopy, '/');
                    if (lastSlash) {
                        *lastSlash = '\0';
                        MakeDirectoryTree(dirCopy);
                    }
                    free(dirCopy);
                }
            }

            /* --- CASE 1: Directory --- */
            if (S_ISDIR(e.mode)) {
                MakeDirectoryTree(targetPath);
                if (cfg->preserveTime) {
                    DirTimeNode* node = (DirTimeNode*)malloc(sizeof(DirTimeNode));
                    if (node) {
                        node->path = _strdup(targetPath);
                        node->mtime = e.mtime;
                        node->next = dirTimeStack;
                        dirTimeStack = node;
                    }
                }
                StreamSkip(in, e.fileSize + e.dataPad);
                if (cfg->verbose) printf("%s\n", targetPath);
            }
            /* --- CASE 2: Linux Symbolic Link (S_IFLNK) --- */
            else if (S_ISLNK(e.mode)) {
                char* linkTarget = (char*)malloc((size_t)e.fileSize + 1);
                if (linkTarget && fread(linkTarget, 1, (size_t)e.fileSize, in) == e.fileSize) {
                    linkTarget[e.fileSize] = '\0';

                    struct _stat64 targetSt;
                    bool isTargetDir = (Stat64Utf8(linkTarget, &targetSt) == 0 && S_ISDIR(targetSt.st_mode));

                    /* Delete existing file before creating symlink */
                    UnlinkUtf8(targetPath);

                    if (!CreateSymlinkUtf8(targetPath, linkTarget, isTargetDir)) {
                        /* Fallback: Save target string as text file if developer mode is off */
                        FILE* out = FileOpenUtf8(targetPath, "w");
                        if (out) {
                            fputs(linkTarget, out);
                            fclose(out);
                        }
                        if (cfg->verbose) printf("%s -> %s (extracted as linkfile)\n", targetPath, linkTarget);
                    } else {
                        if (cfg->verbose) printf("%s -> %s\n", targetPath, linkTarget);
                    }
                    free(linkTarget);
                } else {
                    free(linkTarget);
                    StreamSkip(in, e.fileSize);
                }
                StreamSkip(in, e.dataPad);
            }
            /* --- CASE 3: Hardlink Resolution --- */
            else if (e.nlink > 1 && InodeTable_Find(inodeTable, e.devMin, e.ino) != NULL) {
                const char* primaryPath = InodeTable_Find(inodeTable, e.devMin, e.ino);
                UnlinkUtf8(targetPath);

                if (!CreateHardLinkUtf8(targetPath, primaryPath)) {
                    /* Non-NTFS or volume restriction fallback: duplicate file */
                    CopyFileFallback(primaryPath, targetPath);
                }

                if (cfg->verbose) printf("%s -> %s (hardlink)\n", targetPath, primaryPath);
                StreamSkip(in, e.fileSize + e.dataPad);
            }
            /* --- CASE 4: Regular File --- */
            else {
                bool proceedWithWrite = true;
                struct _stat64 existingStat;

                if (Stat64Utf8(targetPath, &existingStat) == 0) {
                    if (!cfg->unconditional && existingStat.st_mtime >= (time_t)e.mtime) {
                        proceedWithWrite = false;
                    } else {
                        DWORD attr = GetFileAttrUtf8(targetPath);
                        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY)) {
                            SetFileAttrUtf8(targetPath, attr & ~FILE_ATTRIBUTE_READONLY);
                        }
                    }
                }

                if (proceedWithWrite) {
                    FILE* out = FileOpenUtf8(targetPath, "wb");
                    if (!out) {
                        fprintf(stderr, "cpio: cannot write '%s'\n", targetPath);
                        SetWarning();
                        StreamSkip(in, e.fileSize);
                    } else {
                        char buf[BUFFER_CHUNK_SIZE];
                        size_t remaining = (size_t)e.fileSize;
                        bool writeFailed = false;

                        while (remaining > 0) {
                            if (g_AbortRequested) { writeFailed = true; break; }
                            size_t toRead = remaining < sizeof(buf) ? remaining : sizeof(buf);
                            size_t readCount = fread(buf, 1, toRead, in);
                            if (readCount == 0) { writeFailed = true; break; }
                            if (!WriteAll(out, buf, readCount)) { writeFailed = true; break; }
                            remaining -= readCount;
                        }
                        fclose(out);

                        if (writeFailed) {
                            fprintf(stderr, "cpio: fatal: aborted or write failure on '%s'\n", targetPath);
                            UnlinkUtf8(targetPath);
                            free(targetPath);
                            free(rawName);
                            SetFatal();
                            goto cleanup_exit;
                        }

                        if (cfg->preserveTime) {
                            struct __utimbuf64 ut;
                            ut.actime = (time_t)e.mtime;
                            ut.modtime = (time_t)e.mtime;
                            Utime64Utf8(targetPath, &ut);
                        }

                        if ((e.mode & 0222) == 0) {
                            DWORD attr = GetFileAttrUtf8(targetPath);
                            if (attr != INVALID_FILE_ATTRIBUTES) {
                                SetFileAttrUtf8(targetPath, attr | FILE_ATTRIBUTE_READONLY);
                            }
                        }

                        /* Register in hardlink table for sibling links */
                        if (e.nlink > 1) {
                            InodeTable_Add(inodeTable, e.devMin, e.ino, targetPath);
                        }
                    }
                } else {
                    StreamSkip(in, e.fileSize);
                }
                StreamSkip(in, e.dataPad);
                if (cfg->verbose) printf("%s\n", targetPath);
            }
            free(targetPath);
        }
        totalBytes += (e.fileSize + e.dataPad);
        free(rawName);
    }

    if (!foundTrailer && totalBytes > 0 && g_ExitStatus == 0 && !g_AbortRequested) {
        fprintf(stderr, "cpio: warning: archive missing terminal trailer\n");
        SetWarning();
    }

cleanup_exit:
    /* Restore directory modification times in reverse hierarchy (LIFO) */
    while (dirTimeStack != NULL) {
        DirTimeNode* next = dirTimeStack->next;
        if (cfg->preserveTime && g_ExitStatus != 2) {
            struct __utimbuf64 ut;
            ut.actime = (time_t)dirTimeStack->mtime;
            ut.modtime = (time_t)dirTimeStack->mtime;
            Utime64Utf8(dirTimeStack->path, &ut);
        }
        free(dirTimeStack->path);
        free(dirTimeStack);
        dirTimeStack = next;
    }

    InodeTable_Free(inodeTable);
    if (in != stdin) fclose(in);

    if (g_ExitStatus != 2) {
        unsigned long long blockSize = cfg->blockFactor5120 ? 5120 : 512;
        unsigned long long blocks = (totalBytes + (blockSize - 1)) / blockSize;
        fprintf(stderr, "%llu blocks\n", blocks);
    }
    return g_ExitStatus;
}

/* ========================================================================= */
/* MODE: PASS-THROUGH (-p)                                                   */
/* ========================================================================= */

int ExecutePassThrough(const CpioConfig* cfg) {
    MakeDirectoryTree(cfg->targetDir);

    int delim = cfg->nullTerminated ? '\0' : '\n';
    char* line = NULL;
    DirTimeNode* dirTimeStack = NULL;

    while (!g_AbortRequested && (line = ReadStreamLine(stdin, delim)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }

        char* sanitized = NULL;
        if (!SanitizePath(line, &sanitized)) {
            SetWarning();
            free(line);
            continue;
        }

        size_t destLen = strlen(cfg->targetDir) + strlen(sanitized) + 2;
        char* dest = (char*)malloc(destLen);
        if (!dest) {
            free(sanitized);
            free(line);
            SetFatal();
            goto cleanup_passthrough;
        }

        snprintf(dest, destLen, "%s/%s", cfg->targetDir, sanitized);
        NormalizeSlashes(dest);
        free(sanitized);

        struct _stat64 st;
        if (Stat64Utf8(line, &st) != 0) {
            fprintf(stderr, "cpio: cannot access '%s'\n", line);
            SetWarning();
            free(dest);
            free(line);
            continue;
        }

        if (IsSamePhysicalFile(line, dest)) {
            fprintf(stderr, "cpio: skipped: '%s' and '%s' are identical\n", line, dest);
            SetWarning();
            free(dest);
            free(line);
            continue;
        }

        if (cfg->makeDirs) {
            char* dirCopy = _strdup(dest);
            if (dirCopy) {
                char* slash = strrchr(dirCopy, '/');
                if (slash) {
                    *slash = '\0';
                    MakeDirectoryTree(dirCopy);
                }
                free(dirCopy);
            }
        }

        if (S_ISDIR(st.st_mode)) {
            MakeDirectoryTree(dest);
            if (cfg->preserveTime) {
                DirTimeNode* node = (DirTimeNode*)malloc(sizeof(DirTimeNode));
                if (node) {
                    node->path = _strdup(dest);
                    node->mtime = (uint32_t)st.st_mtime;
                    node->next = dirTimeStack;
                    dirTimeStack = node;
                }
            }
        } else {
            FILE* src = FileOpenUtf8(line, "rb");
            if (!src) {
                fprintf(stderr, "cpio: open failed on source '%s'\n", line);
                SetWarning();
                free(dest);
                free(line);
                continue;
            }

            DWORD attr = GetFileAttrUtf8(dest);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY)) {
                SetFileAttrUtf8(dest, attr & ~FILE_ATTRIBUTE_READONLY);
            }

            FILE* dst = FileOpenUtf8(dest, "wb");
            if (!dst) {
                fclose(src);
                fprintf(stderr, "cpio: failed to create destination '%s'\n", dest);
                SetWarning();
                free(dest);
                free(line);
                continue;
            }

            char buf[BUFFER_CHUNK_SIZE];
            size_t bytes;
            bool copyFailed = false;

            while ((bytes = fread(buf, 1, sizeof(buf), src)) > 0) {
                if (g_AbortRequested) { copyFailed = true; break; }
                if (!WriteAll(dst, buf, bytes)) { copyFailed = true; break; }
            }

            fclose(src);
            fclose(dst);

            if (copyFailed) {
                fprintf(stderr, "cpio: fatal: write failure copying to '%s'\n", dest);
                UnlinkUtf8(dest);
                free(dest);
                free(line);
                SetFatal();
                goto cleanup_passthrough;
            }

            if (cfg->preserveTime) {
                struct __utimbuf64 ut;
                ut.actime = st.st_atime;
                ut.modtime = st.st_mtime;
                Utime64Utf8(dest, &ut);
            }

            if ((st.st_mode & _S_IWRITE) == 0) {
                DWORD dAttr = GetFileAttrUtf8(dest);
                if (dAttr != INVALID_FILE_ATTRIBUTES) {
                    SetFileAttrUtf8(dest, dAttr | FILE_ATTRIBUTE_READONLY);
                }
            }
        }

        if (cfg->verbose) printf("%s -> %s\n", line, dest);
        free(dest);
        free(line);
    }

cleanup_passthrough:
    while (dirTimeStack != NULL) {
        DirTimeNode* next = dirTimeStack->next;
        if (cfg->preserveTime && g_ExitStatus != 2) {
            struct __utimbuf64 ut;
            ut.actime = (time_t)dirTimeStack->mtime;
            ut.modtime = (time_t)dirTimeStack->mtime;
            Utime64Utf8(dirTimeStack->path, &ut);
        }
        free(dirTimeStack->path);
        free(dirTimeStack);
        dirTimeStack = next;
    }

    return g_ExitStatus;
}
