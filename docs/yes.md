# yes

## What it does

Repeatedly writes a line containing `y`, or the strings supplied on the command line, until interrupted or until the output destination closes.

## Usage

```text
yes [STRING]...
yes OPTION
```

With no string arguments, `yes` writes `y`. Multiple string arguments are joined with single spaces before the trailing newline is added.

## Options

- `-h`, `--help`: show help text and exit.
- `-v`, `--version`: show version information and exit.
- `--`: treat all following arguments as output strings, including values beginning with `-`.

## Runtime behavior

- Writes output through a fixed 64 KiB buffer when the line fits.
- Uses native Windows output handles.
- Exits cleanly on Ctrl-C or Ctrl-Break.
- Exits cleanly when a downstream pipe closes, such as in a bounded pipeline.
- Returns a non-zero status when standard output cannot be acquired; output-loop failures terminate cleanly.

## Examples

```text
yes
yes confirm
yes -- -h
```

## UNIX origin

A standard Unix utility used to repeatedly output text, commonly for feeding confirmations or stress-testing pipelines.
