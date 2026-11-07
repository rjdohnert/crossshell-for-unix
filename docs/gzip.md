# gzip

`gzip` compresses files with the DEFLATE algorithm and the `.gz` format, or
decompresses existing gzip files.

## Usage

```text
gzip [OPTIONS] FILE...
gzip -c [OPTIONS] [FILE...]
gzip [OPTIONS] < input > output
```

| Option | Description |
| --- | --- |
| `-d`, `--decompress`, `--uncompress` | Decompress instead of compress. |
| `-k`, `--keep` | Keep input files after successful file-to-file processing. |
| `-f`, `--force` | Replace an existing regular output file. |
| `-c`, `--stdout` | Write the result to standard output and preserve file inputs. |
| `--json`, `--csv`, `--table` | Select the operation-report format. |
| `--pipe COMMAND` | Send the operation report to a command. |
| `-h`, `--help` | Display command help. |
| `-V`, `--version` | Display version information. |

With no file arguments, `gzip` reads standard input and writes standard output.
Multiple files passed with `-c` are emitted as concatenated gzip members; the
decompressor accepts and processes concatenated members in order.

## Safety and compatibility

- File-to-file operations write to a temporary file beside the destination and
  install the completed result only after compression or decompression succeeds.
- Input files are removed only after the output has been installed. `-k` and
  standard-output mode preserve the inputs.
- Existing outputs are not replaced unless `-f` is supplied. Directories,
  devices, and reparse-point outputs are refused.
- Gzip headers with optional fields, including header checksums, and stored,
  fixed-Huffman, and dynamic-Huffman DEFLATE blocks are supported. Member
  checksums are validated during decompression.
- Standard input is spooled to a temporary file before processing.
- This Windows implementation uses Windows PowerShell's
  `System.IO.Compression.GZipStream` runtime.

## Build

Run `src\gzip\build.ps1` on Windows with `clang++` available. The build compiles
the modules in `src\gzip` and copies `gzip.exe` into `bin\`, where the installer
collects the executable.
