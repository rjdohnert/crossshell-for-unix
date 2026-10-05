// ============================================================================
// Comprehensive Help Documentation (BSD / Unix Manual Format)
// ============================================================================

const char* const HELP_MANUAL =
R"(NAME
     install -- robust file and directory tree copy

SYNOPSIS
     install [options] <source> <destination> [file [file ...]]

DESCRIPTION
     install replicates directory hierarchies and files with high resiliency.

OPTIONS
   Copy & Directory Tree
     -s, --subdirs
             Copy subdirectories, excluding empty ones.
     -e, --all-subdirs
             Copy subdirectories, including empty ones.
     -m, --mirror
             Mirror directory tree (equivalent to -e plus --purge).
     --purge
             Delete destination files and folders that do not exist in source.
     -l <n>, --level=<n>
             Only traverse the top <n> levels of the source directory tree.
     --move-files
             Move files (delete source files after successful copy).
     --move-all
             Move files and directories (delete source tree after copy).
     --create-zero
             Create target directory tree and zero-length files only.

   File Selection & Filtering
     --exclude-changed
             Exclude changed files (identical timestamp, differing size).
     --exclude-newer
             Exclude newer files (source files newer than destination).
     --exclude-older
             Exclude older files (source files older than destination).
     --exclude-files=<pattern>
             Exclude files matching the wildcard pattern (e.g. *.tmp).
     --exclude-dirs=<pattern>
             Exclude directories matching the pattern (e.g. .git).
     --max-size=<bytes>
             Exclude files larger than <bytes>.
     --min-size=<bytes>
             Exclude files smaller than <bytes>.

   Retry & Fault Tolerance
     -r <n>, --retries=<n>
             Number of retries on failed copies (default: 3).
     -w <n>, --wait=<n>
             Wait time between retries in seconds (default: 5).
     -j <n>, -t <n>, --threads=<n>
             Do multithreaded copies with <n> worker threads (default: 1).

   Logging & Output Formatting
     -n, -L, --dry-run
             List only; do not copy, modify, or delete any files.
     -v, --verbose
             Verbose output; displays skipped and identical files.
     --no-progress
             Suppress copy progress indicator.
     --no-file-list
             Do not log individual file names.
     --no-dir-list
             Do not log directory names.
     --no-header
             Suppress Install job header banner.
     --no-summary
             Suppress tabular execution statistics summary.
     -h, --help
             Display this manual documentation and exit.
     -V, --version
             Display version information and exit.

EXIT STATUS
     The exit code is a bitmap reflecting execution results (Install standard):
     0x00 (0)    No errors occurred, and no files were copied.
     0x01 (1)    One or more files were successfully copied.
     0x02 (2)    Extra files or directories were detected at the destination.
     0x04 (4)    Mismatched files or directories were detected.
     0x08 (8)    One or more copy operations failed after max retries.
     0x10 (16)   Fatal error: invalid parameters or unreadable source path.

EXAMPLES
     Mirror source directory to backup storage using 8 threads:
           $ install -m -j 8 C:\Workspace D:\Backups\Workspace

     Incremental sync excluding object files and Git directory:
           $ install -e --exclude-files=*.obj --exclude-dirs=.git C:\Src C:\Dst
)";

