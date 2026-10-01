# env

## What it does
Prints the environment or runs a command with modified environment variables.

## Usage
`env [OPTION]... [-] [NAME=VALUE]... [COMMAND [ARG]...]`

## Options
- `-i`, `--ignore-environment`: start with an empty environment
- `-u NAME`, `--unset=NAME`: remove a variable
- `--help`: show help text
- `--version`: show version information

## Behavior
- Without `COMMAND`, prints the resulting environment as `NAME=VALUE` lines.
- With `COMMAND`, executes it and returns the child process exit code.

## UNIX origin
Standard Unix environment utility.
