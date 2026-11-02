#include "cpio_cli.h"

/* ========================================================================= */
/* COMMAND-LINE PARSER & DOCUMENTATION                                       */
/* ========================================================================= */

void DisplayHelp(void) {
    puts(
"NAME\n"
"    cpio - Universal Archive Engine\n"
"\n"
"SYNOPSIS\n"
"    cpio -o [qualifiers...] [-F archive] < name-list\n"
"    cpio -i [qualifiers...] [-F archive] [pattern...]\n"
"    cpio -t [qualifiers...] [-F archive] [pattern...]\n"
"    cpio -p [qualifiers...] destination-directory < name-list\n"
"\n"
"OPERATIONAL MODES (Mutually Exclusive):\n"
"    -o, --create             Generate archive from list on standard input\n"
"    -i, --extract            Extract components from archive\n"
"    -t, --list               Display archive Table of Contents\n"
"    -p, --pass-through DIR   Replicate file structure directly to DIR\n"
"\n"
"QUALIFIERS:\n"
"    -v, --verbose            Produce detailed activity and metadata log\n"
"    -d, --make-directories   Construct directory hierarchies automatically\n"
"    -u, --unconditional      Force overwrite even if existing file is newer\n"
"    -m, --preserve-mtime     Retain original modification timestamps\n"
"    -0, --null               Parse null-delimited (\\0) input streams\n"
"    -B                       Set I/O block factor to 5120 bytes (default: 512)\n"
"    -F, --file=FILE          Read from/write to FILE instead of std streams\n"
"    -H, --format=FORMAT      Archive layout on create: 'newc' (default), 'crc', 'odc'\n"
"    -?, --help               Display this documentation reference\n"
"        --version            Print version information\n"
"\n"
"SUPPORTED ARCHIVE FORMATS (Auto-detected on read):\n"
"    * SVR4 Portable ASCII without CRC (070701 - newc / Linux initramfs)\n"
"    * SVR4 Portable ASCII with CRC (070702 - crc / RPM payload)\n"
"    * POSIX.1 Portable ASCII (070707 - odc / BSD)\n"
"    * Old Binary Format (0x71C7, 0xC771 - Little & Big Endian)\n"
"\n"
"UNIX SUBSYSTEM PARITY:\n"
"    * Resolves duplicate inodes and builds NTFS hardlinks (CreateHardLinkW).\n"
"    * Restores Unix symbolic links (CreateSymbolicLinkW) with linkfile fallback.\n"
"    * Handles path hierarchies up to 32,767 characters (PrepareWin32Path)."
    );
}

void CleanupConfig(CpioConfig* cfg) {
    if (cfg->patterns) {
        for (int k = 0; k < cfg->patternCount; ++k) free(cfg->patterns[k]);
        free(cfg->patterns);
        cfg->patterns = NULL;
    }
    if (cfg->archivePath) { free(cfg->archivePath); cfg->archivePath = NULL; }
    if (cfg->targetDir) { free(cfg->targetDir); cfg->targetDir = NULL; }
}
