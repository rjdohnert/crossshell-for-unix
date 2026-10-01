# mv

## What it does
Moves or renames files and directories, with native Windows rename behavior and a cross-volume fallback.

## Supported options
- `-f`: force overwrite of an existing target without prompting
- `-i`: prompt before overwriting an existing target
- `-e`: preserve Windows attributes and security descriptors during cross-filesystem moves
- `-v`, `--verbose`: show extra operational notices
- `--no-progress`: disable the progress bar during copy fallback moves
- `--json`: emit one line-delimited JSON object for each successful move, including source and destination
- `-h`, `--help`: show help text

## Behavior
- If the source and destination are on the same volume, it tries an atomic rename.
- If the move crosses volumes, it copies the data and removes the source afterward.
- When the target exists and overwrite is approved, the destination is replaced successfully.
- In `--json` mode the command emits structured records instead of text progress output, which works well with PowerShell `ConvertFrom-Json` and jq.

## Examples

```text
mv update.tar update_old.tar
mv file1.log file2.log C:\Archive\
mv -i database.mdf D:\LiveMount\
mv -fe SourceTree E:\BackupTree\
mv --json oldname.txt newname.txt
```

## UNIX origin
A core Unix file-management utility from early BSD and System V releases.
