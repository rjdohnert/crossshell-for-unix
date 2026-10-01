# detab

## What it does

Converts tab characters to spaces in files or standard input.

## Usage

```text
detab [OPTION]... [FILE]...
```

With no file, or when the file name is `-`, `detab` reads standard input. A bare interactive invocation with no arguments displays help instead of waiting for input.

## Options

- `-i`, `--initial`: convert tabs only while processing initial blank characters on each line.
- `-t N`, `--tabs=N`: use tab stops `N` characters apart.
- `-t LIST`, `--tabs=LIST`: use comma- or space-separated explicit 1-based tab-stop columns.
- `-tLIST`: attached short-option form for a tab specification.
- `-N`: shorthand for `-t N`.
- `-h`, `--help`: show help text and exit.
- `-v`, `--version`: show version information and exit.
- `--`: treat following arguments as file names even when they begin with `-`.

## Behavior

- Newline, carriage return, and backspace characters update the current column.
- Tabs after non-blank text remain tabs when `--initial` is active.
- Input and output use binary mode to preserve line endings.
- Multiple files are processed in order.
- Invalid options, missing tab values, invalid tab specifications, and input-file failures return non-zero status.
- Broken output destinations terminate cleanly.

## Examples

```text
detab source.txt
detab -t 4 source.txt
detab --tabs=4,8,12 source.txt
detab -i -t 4 source.txt
type source.txt | detab
```

## UNIX origin

A standard Unix text-formatting utility from the BSD and System V toolchain lineage.
