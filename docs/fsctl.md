# fsctl

## What it does
`fsctl` is an interactive file manager shell for local and remote paths.

It can browse directories, inspect files, create/remove files and folders, copy/move files, run recursive search/tree listings, and work with remote FTP/SSH paths.

## Interactive commands
- `drive`: list available drives with storage information
- `shares` / `net`: list available network shares
- `ls` / `dir [path] [-t|--tree]`: list directory contents, optionally in tree mode
- `tree [path]`: recursively print directory tree
- `cd <directory-or-remote-uri>`: change current path
- `pwd`: print current path
- `mkdir <name>`: create directory (creates parent directories as needed)
- `rmdir <name>`: remove an empty directory (confirmation controlled by `confirm`)
- `trash [-f|--force] <path> [path2 ...]`: move file/folder(s) to recycle bin (`-f` skips confirmation prompts)
- `rm [-r|--recursive] [-f|--force] <path>`: remove files, or directories when `-r` is used (`-f` skips confirmations)
- `touch [-c|--no-create] <file> [file2 ...]`: create file(s) if missing, or update modified timestamp(s); with `-c`, only update existing files
- `copy <src> <dst>`: copy a file
- `move <src> <dst>`: move/rename a file
- `cat` / `type <file>`: print file content
- `stat <path>`: show path metadata
- `find <pattern>`: recursively search for matching names
- `diff <file1> <file2>`: compare two local text files line by line
- `history [clear]`: show command history, or clear it
- `confirm [on|off|status]`: toggle confirmation prompts for destructive actions
- `exit` / `quit`: close fsctl shell

## Options
- In-shell `help`: show command list and examples

## Remote protocol support
- `ftp://host/path`: browse/list/read via FTP
- `ssh://user@host/path`: browse/list/read via SSH (requires `ssh.exe` or `plink.exe`)

## Output redirection
- `>` overwrite command output file
- `>>` append command output file

## UNIX origin
A compatibility-style utility rather than a core command from a single historical Unix release.
