# mkfs

Formats a mounted local Windows volume as NTFS, FAT32, or exFAT. The target is a
drive letter; `mkfs` does not create, resize, or partition disks.

## Usage

```text
mkfs [options] <drive>
```

The drive can be written as `E`, `E:`, `E:\`, or `E:/`. Only one drive target
may be specified.

## Options

| Option | Description |
| --- | --- |
| `-t`, `--type <type>` | File system: `NTFS`, `FAT32`, or `exFAT` (default: `NTFS`). |
| `-L`, `--label <label>` | Set the volume label. Maximum lengths are 32 characters for NTFS, 11 for FAT32, and 15 for exFAT. |
| `-c`, `--cluster <bytes>` | Request a cluster size of 512, 1024, 2048, 4096, 8192, 16384, 32768, or 65536 bytes. Windows may reject a size that is not valid for the selected file system or volume. |
| `-q`, `--quick` | Perform a quick format (default). |
| `-f`, `--full` | Perform a full format. Cannot be combined with `-q`. |
| `-y`, `--yes` | Skip the confirmation prompt. |
| `-v`, `--verbose` | Display the target and selected settings. |
| `-h`, `--help` | Display help. |
| `-V`, `--version` | Display version information. |

## Safety and compatibility

Formatting erases existing data on the selected volume. `mkfs` asks for
confirmation by default; `--yes` bypasses that prompt. Check the drive letter
carefully before confirming. The target must be a local fixed, removable, or
RAM-disk volume. Formatting may require an elevated console.

Formatting is performed through `FormatEx` in the Windows system
`fmifs.dll`. This is an internal Windows interface, so its availability and
behavior may vary between Windows versions.

## Examples

```text
mkfs E:
mkfs --type FAT32 --label USB_STICK F:
mkfs --type exFAT --cluster 65536 -y G:
mkfs --type NTFS --full D:
```
