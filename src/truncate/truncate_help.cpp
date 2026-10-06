#include "truncate_help.hpp"

const char* const BSD_MANUAL =
R"(NAME
     truncate -- truncate or extend the length of files

SYNOPSIS
     truncate [-c] -s [+|-|%|/]size[b|k|m|g|t|p|e] file ...
     truncate [-c] -r rfile file ...
     truncate [-h | -V]

DESCRIPTION
     The truncate utility shrinks or extends the length of each target file to
     the specified size.

     If a target file does not exist, it will be created unless the -c option
     is specified. If a file is extended, the newly allocated region reads as
     zero bytes (and creates sparse allocations on NTFS where applicable).

OPTIONS
     -c      Do not create files if they do not exist. The truncate utility
             treats non-existent files as non-errors when -c is specified.

     -r rfile
             Truncate or extend target files to match the exact size of the
             reference file rfile.

     -s [+|-|%|/]size[b|k|m|g|t|p|e]
             Set the target file size. The size argument can be prefixed with
             one of the following operational modifiers:

             +     Increase the current length of the file by size bytes.
             -     Decrease the current length of the file by at most size
                   bytes (the length will never be made negative).
             %     Round the current length down to a multiple of size bytes.
             /     Round the current length up to a multiple of size bytes.

             Without a prefix, the file size is set to exact size.

             The size argument may be followed by a case-insensitive unit suffix:
                   b      512-byte blocks
                   k, K   Kilobytes (1024 bytes)
                   m, M   Megabytes (1048576 bytes)
                   g, G   Gigabytes (1073741824 bytes)
                   t, T   Terabytes (1099511627776 bytes)
                   p, P   Petabytes (1125899906842624 bytes)
                   e, E   Exabytes  (1152921504606846976 bytes)

     -h, --help
             Display this manual help page and exit.

     -V, --version
             Display version information and exit.

EXIT STATUS
     The truncate utility exits 0 on success, and >0 if an error occurs.

EXAMPLES
     Set file.bin to exactly 10 Megabytes:
           $ truncate -s 10M file.bin

     Grow existing logs by 500 Kilobytes without creating if absent:
           $ truncate -c -s +500K server.log

     Round file.bin down to a multiple of 4 Kilobytes (4096-byte block alignment):
           $ truncate -s %4K file.bin

     Make target.dat match the exact size of template.dat:
           $ truncate -r template.dat target.dat

SEE ALSO
     SetEndOfFile(3), SetFilePointerEx(3), dd(1)
)";
