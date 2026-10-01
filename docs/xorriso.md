# xorriso

## What it does

Builds ISO images in a `mkisofs`-compatible mode, extracts files from ISO images, and inspects ISO volume metadata. It is a Windows-native and single-file implementation focused on the most common xorriso-style workflows.

## Usage

- `xorriso.exe -as mkisofs -o <output.iso> -V <label> <source_dir>`
- `xorriso.exe -extract <input.iso> <target_dir>`
- `xorriso.exe -indev <input.iso> -toc`
- `xorriso.exe -help`

## Options

- `-as mkisofs`: create an ISO image from a source directory in mkisofs compatibility mode
- `-o <path>`: set the output ISO file path
- `-V <label>`: set the volume ID or label
- `-extract <iso> <dir>`: extract an ISO file tree to a local directory
- `-indev <iso>`: inspect an ISO image and print its volume metadata
- `-toc`: show the table of contents / volume metadata when combined with `-indev`
- `-v`: enable verbose logging
- `-help`, `/?`: show help

## Notes

- The implementation is intentionally compact and single-file for this repository.
- It supports the core workflows required for ISO creation, extraction, and metadata inspection on Windows.
- It is not a full xorriso replacement and should be treated as a practical subset aimed at Windows automation and imaging workflows.

## Examples

- `xorriso.exe -as mkisofs -o C:\temp\build.iso -V "MY_LABEL" C:\source_dir`
- `xorriso.exe -extract C:\temp\build.iso C:\temp\extract`
- `xorriso.exe -indev C:\temp\build.iso -toc`
