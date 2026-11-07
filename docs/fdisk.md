# fdisk

## What it does
Lists and edits Windows physical-disk and VHD/VHDX partition tables. It supports
MBR primary, extended, and logical partitions, plus GPT partitions. Filesystem
formatting is handled by separate tools.

## Usage
- `fdisk <disk>`: open the interactive partition-table editor
- `fdisk -l [<disk>]`: list partitions on all disks or one disk
- `fdisk --image <path> [--list]`: attach a VHD/VHDX and edit it, or list it
- `fdisk --create-image <path.vhd|path.vhdx> --size <MiB> [--format vhd|vhdx]`
- `fdisk --resize-image <path.vhd|path.vhdx> --size <MiB>`
- `fdisk --detach-image <path.vhd|path.vhdx>`
- `-u`, `--sectors`: display partition locations in sectors instead of cylinders
- `-h`, `--help`: show help
- `-V`, `--version`: show version

Disk targets may be a decimal index, `PhysicalDriveN`, or
`\\.\PhysicalDriveN`.

Image creation produces a dynamically expanding image; the format is inferred
from the `.vhd` or `.vhdx` extension unless `--format` is specified. Capacity is
given in MiB. Image editing attaches it without assigning drive letters, and it
remains attached after fdisk exits; use `--detach-image` when finished. Attaching
or editing images may require an elevated console. Image capacity changes require
the image to be detached. Shrinks use Windows' safe resize checks and are
rejected if they would truncate existing partitions.

## Editor commands
- `p`: print the current layout
- `n`: create a partition; for MBR, choose primary, extended, or logical
- `d`: delete a partition
- `t`: change the MBR type byte or choose a supported GPT type
- `a`: toggle the active flag on a primary MBR partition
- `s`: resize a partition extent
- `g`, `o`: stage a new GPT or MBR partition table
- `w`: write staged changes after typing `WRITE`
- `q`: discard staged changes and exit

Changes remain in memory until `w` is confirmed. Resizing changes the partition
extent only; it does not resize or format the filesystem. Raw-disk writes
typically require an elevated console. Before writing, fdisk attempts to lock
and dismount volumes on the selected disk; it refuses the write if those
volumes cannot be made safe. Do not initialize a disk or write a layout unless
the selected physical disk is correct and its data is backed up.

VHDX and virtual-disk resize support depend on the Windows Virtual Disk API
available on Windows 8 / Windows Server 2012 and newer.

## UNIX origin
A classic Unix disk-partitioning utility from BSD and System V-style systems.
