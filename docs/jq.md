# jq

Processes JSON input with a filter and writes the results to standard output.

## Usage

```text
jq [options] <filter> [file...]
```

When no input files are provided, jq reads standard input. If no filter is
provided, the identity filter `.` is used.

```powershell
'{"name":"Ada","active":true}' | jq '.name'
jq -c '.items[]' data.json
```

## Options

- `-c`, `--compact-output`: emit compact JSON.
- `-n`, `--null-input`: use `null` as the input value.
- `-e`, `--exit-status`: set the exit status from the result's truthiness.
- `-s`, `--slurp`: read all inputs into an array.
- `-r`, `--raw-output`: write strings without JSON quotes.
- `-j`, `--join-output`: raw output without a newline after each result.
- `-R`, `--raw-input`: read input lines as strings.
- `-S`, `--sort-keys`: sort object keys in output.
- `-C`, `--color-output`; `-M`, `--monochrome-output`: enable or disable color.
- `-a`, `--ascii-output`: escape non-ASCII characters.
- `--tab`, `--indent N`: choose tab indentation or `N` spaces (default `2`).
- `--unbuffered`: flush output after each JSON result.
- `-f`, `--from-file FILE`: read the filter from a file.
- `-h`, `--help`: show help; `-V`, `--version`: show version.