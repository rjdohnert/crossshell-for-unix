# expr

## What it does
Evaluates an expression and prints the result.

## Usage
`expr EXPRESSION`

## Supported operators
- Logical: `|`, `&`
- Comparison: `=`, `!=`, `<`, `<=`, `>`, `>=`
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Parentheses: `(`, `)`

## Exit status
- `0`: result is non-empty and not `0`
- `1`: result is empty or `0`
- `2`: syntax or evaluation error

## Options
- `--help`: show help text
- `--version`: show version information

## UNIX origin
Standard Unix expression evaluator.
