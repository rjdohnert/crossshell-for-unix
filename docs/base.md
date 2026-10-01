# base

## What it does
Encodes or decodes standard input or one input file using RFC 4648 Base64 or Base32 and writes the result to standard output or an output file.

## Usage
```text
base [OPTION]... [FILE]
```

With no input file, `base` reports an error and suggests `base --help`. Use `-` as `FILE` to read standard input explicitly.

## Options
- `-64`, `--base64`: use Base64 encoding or decoding (default).
- `-32`, `--base32`: use Base32 encoding or decoding.
- `-d`, `--decode`: decode the input instead of encoding it.
- `-i`, `--ignore-garbage`: ignore non-alphabet characters while decoding.
- `-w COLS`, `--wrap=COLS`: wrap encoded output after `COLS` characters; the default is 76 and `0` disables wrapping.
- `-o FILE`, `--output=FILE`: write output to `FILE`.
- `-h`, `--help`: display help text.
- `-v`, `--version`: display version information.

## Examples
```text
echo Hello, World! | base
base -32 -w 0 input.bin -o output.b32
base -d -i encoded.txt -o recovered.bin
echo JBSWY3DPEBLW64TMMQ====== | base -32 -d
```

## Exit status
Returns `0` on success. Returns `1` for invalid arguments, input/output failures, or invalid encoded data.

## Windows notes
The command uses binary mode for standard input and output to prevent newline translation during binary round trips.

## UNIX origin
Based on the BSD coreutils `base64` command, extended with RFC 4648 Base32 support.
