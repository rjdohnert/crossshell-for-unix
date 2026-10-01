# dskctl

## What it does
Provides a preview-first Windows cleanup utility for temporary and cache-related files. It can inspect and optionally remove data from known temporary locations, Windows update caches, crash dumps, log directories, and the Recycle Bin.

## Usage
- `dskctl [options] [targets...]`

## Options
- `-a`, `--all`: select all available cleanup targets, including higher-risk ones
- `-s`, `--safe`: select only the safer, non-destructive cleanup targets
- `-n`, `--dry-run`: preview cleanup actions without removing files (default)
- `-x`, `--execute`: perform the cleanup actions after previewing
- `-t`, `--trash`: move items to the Recycle Bin instead of deleting them permanently
- `-v`, `--verbose`: print the full path of each evaluated file or directory
- `-f`, `--force`: bypass the interactive confirmation prompts
- `-h`, `--help`: show help text and exit

## Targets
- `--temp`: cleans the current user temp directory
- `--system-temp`: cleans the Windows system temp directory
- `--update-cache`: cleans Windows Update download packages
- `--error-dumps`: removes memory dump and minidump files
- `--logs`: removes Windows setup and system log files
- `--recycle-bin`: empties the Recycle Bin
- `--wer`: removes Windows Error Reporting crash dumps and queues
- `--shaders`: removes DirectX shader cache files
- `--delivery-opt`: removes Delivery Optimization cache files
- `--inet-cache`: removes Internet cache temporary files

## Safety notes
- `dskctl` requires elevated Administrator privileges.
- Preview mode is the default, so the tool will not delete anything unless `--execute` is used.
- Cleanup is restricted to a known allowlist of safe Windows temp/cache/log paths.
- The tool prints a preview report and logs the session before any destructive action is taken.

## Examples
- `dskctl --safe`
- `dskctl --temp --logs --execute`
- `dskctl -a -v --trash`
