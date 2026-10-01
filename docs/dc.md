# dc

## What it does
Provides an arbitrary-precision reverse-Polish desk calculator with stack operations, registers, executable macros, configurable scale, and input/output radixes.

## Usage
`dc [-hV] [-e expression] [-f file] [file ...]`

Without an expression or file, `dc` reads commands from standard input.

## Options
- `-e expression`, `--expression=expression`: evaluate an expression before other input
- `-f file`, `--file=file`: read and evaluate commands from a file
- `-h`, `--help`: show help text
- `-V`, `--version`: show version information

## Common commands
- `+`, `-`, `*`, `/`, `%`, `^`, `v`: arithmetic operations
- `p`, `n`, `f`: print the top value, print and pop it, or print the full stack
- `c`, `d`, `r`, `R`, `z`: clear, duplicate, swap, rotate, or measure the stack
- `sX`, `lX`, `SX`, `LX`: store and load register `X` or its register stack
- `k`, `i`, `o`: set scale, input radix, or output radix
- `[commands]x`: define and execute a macro

## Examples
```text
dc -e "10 3 / p"
dc calculations.dc
```

## UNIX origin
Compatible with the traditional Unix `dc` reverse-Polish desk calculator.