#include "sync_help.hpp"

const char* const BSD_MANUAL =
R"(NAME
     sync -- force completion of pending disk writes (flush cache)

SYNOPSIS
     sync [-dfhqrsvV] [file ...]

DESCRIPTION
     The sync utility forces a write of dirty (modified) buffers in the block
     buffer cache out to the underlying physical storage media. On Windows,
     sync coordinates with the Windows Cache Manager and storage subsystem
     to commit dirty cache pages across mounted NTFS, ReFS, FAT32, and exFAT
     volumes via FlushFileBuffers.

     If no files or paths are specified, all active fixed and removable disk
     volumes on the system are synchronized.

OPTIONS
     -d, --data
             Synchronize only file data without metadata updates where
             supported.

     -f, --file-system
             Synchronize the entire file system(s) containing the specified
             files or directories.

     -r, --removable
             Restrict cache synchronization to removable storage media
             (e.g., USB thumb drives, external disks, flash memory cards).

     -v, --verbose
             Verbose mode. Displays diagnostic details for each synchronized
             drive or file, including volume label, file system, and status.

     -q, --quiet
             Quiet mode. Suppress all informational output and warnings.

     -h, --help
             Display this manual help page and exit.

     -V, --version
             Display version information and exit.

OPERANDS
     file ...
             Synchronize the specified files or directory paths. If -f is also
             specified, the volume containing each path is synchronized.

EXIT STATUS
     0       All buffers were successfully synchronized.
     1       One or more volumes or files failed to synchronize, or
             permission was denied.
     2       Invalid command-line arguments.

COMPATIBILITY NOTES

     Note: Flushing entire volume caches requires an elevated Command Prompt
     or PowerShell terminal (Run as Administrator) under Windows UAC.

SEE ALSO
     fsync(2), sync(2), reboot(8), halt(8)
)";
