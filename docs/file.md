# file

## What it does
Identifies the type of each file operand.

## Usage
`file [OPTION]... FILE...`

## Behavior
- Detects common binary signatures (for example PE, ELF, ZIP, gzip, xz, PNG, JPEG, PDF).
- Reports directory or reparse-point metadata.
- Falls back to extension/text heuristics when no signature matches.

## Options
- `--help`: show help text
- `--version`: show version information

## UNIX origin
Classic Unix file-type identification utility.
