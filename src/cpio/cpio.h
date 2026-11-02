#ifndef CPIO_H
#define CPIO_H

// Shared umbrella header for the cpio archive engine: platform includes,
// constants, types, and the global state used across all modules.

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <time.h>
#include <fcntl.h>
#include <io.h>
#include <direct.h>

#define CPIO_TRAILER         "TRAILER!!!"
#define CPIO_HEADER_SIZE     110
#define BUFFER_CHUNK_SIZE    (1024 * 64)    /* 64 KB kernel-aligned I/O buffer */
#define SVR4_MAX_FILE_SIZE   0xFFFFFFFFULL  /* 4 GB architectural limit for SVR4 */
#define MAX_PATH_EXTENDED    32768
#define INODE_TABLE_BUCKETS  8192

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & 0170000) == 0040000)
#endif
#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & 0170000) == 0100000)
#endif
#ifndef S_ISLNK
#define S_ISLNK(mode) (((mode) & 0170000) == 0120000)
#endif

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

typedef enum {
    MODE_NONE,
    MODE_CREATE,      /* -o */
    MODE_EXTRACT,     /* -i */
    MODE_LIST,        /* -t */
    MODE_PASSTHROUGH  /* -p */
} CpioMode;

typedef enum {
    FMT_UNKNOWN,
    FMT_NEWC,    /* SVR4 ASCII without CRC (070701) */
    FMT_CRC,     /* SVR4 ASCII with CRC (070702) */
    FMT_ODC,     /* POSIX.1 / Old Portable ASCII (070707) */
    FMT_BIN_LE,  /* Old Binary Little-Endian (0x71c7) */
    FMT_BIN_BE   /* Old Binary Big-Endian (0xc771) */
} ArchiveFormat;

typedef struct {
    ArchiveFormat format;
    uint32_t      ino;
    uint32_t      mode;
    uint32_t      uid;
    uint32_t      gid;
    uint32_t      nlink;
    uint32_t      mtime;
    uint64_t      fileSize;
    uint32_t      nameSize;
    uint32_t      devMaj;
    uint32_t      devMin;
    uint32_t      check;
    size_t        namePad;
    size_t        dataPad;
} GenericCpioEntry;

typedef struct {
    CpioMode      mode;
    ArchiveFormat createFormat;
    bool          verbose;
    bool          makeDirs;
    bool          unconditional;
    bool          preserveTime;
    bool          nullTerminated;
    bool          blockFactor5120; /* -B */
    char*         archivePath;     /* -F */
    char*         targetDir;       /* -p destination */
    char**        patterns;        /* -i / -t patterns */
    int           patternCount;
    int           patternCapacity;
} CpioConfig;

/* Hardlink tracking hash-node */
typedef struct InodeNode {
    uint32_t          dev;
    uint32_t          ino;
    char*             primaryPath;
    struct InodeNode* next;
} InodeNode;

/* Deferred directory timestamp stack node */
typedef struct DirTimeNode {
    char*               path;
    uint32_t            mtime;
    struct DirTimeNode* next;
} DirTimeNode;

/* Global state for POSIX exit codes and thread-safe signal cleanup */
extern int            g_ExitStatus;
extern volatile LONG  g_AbortRequested;
extern UINT           g_OriginalConsoleCP;
extern UINT           g_OriginalConsoleOutputCP;

void SetWarning(void);
void SetFatal(void);

#endif // CPIO_H
