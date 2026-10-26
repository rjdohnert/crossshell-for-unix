# pv

Copies files or standard input to standard output while reporting transfer
progress, throughput, and estimated completion time.

## Usage

```text
pv [OPTIONS] [FILE]...
```

With no files, pv reads standard input. If no display options are specified,
the display includes a progress bar, elapsed time, byte count, rate, and ETA.

```powershell
pv large-file.zip > copy.zip
producer.exe | pv --size 100M > output.dat
```

## Options

### Display

- `-p`, `--progress`: show the progress bar.
- `-t`, `--timer`: show elapsed time.
- `-e`, `--eta`: show estimated time remaining.
- `-I`, `--fineta`: show estimated finish time.
- `-r`, `--rate`: show current transfer rate.
- `-a`, `--average-rate`: show average transfer rate.
- `-b`, `--bytes`: show transferred bytes or lines.
- `-8`, `--bits`: report rates and counts in bits.
- `-n`, `--numeric`: output only a numeric percentage.
- `-q`, `--quiet`: suppress transfer information.

### Transfer

- `-W`, `--wait`: wait for the first byte before displaying progress.
- `-D SEC`, `--delay-start SEC`: delay the display.
- `-s SIZE`, `--size SIZE`: set the expected input size.
- `-l`, `--line-mode`: count lines instead of bytes.
- `-0`, `--null`: use NUL as the line delimiter.
- `-i SEC`, `--interval SEC`: set the display update interval (default `0.25`).
- `-w WIDTH`, `--width WIDTH`: set the assumed terminal width.
- `-N NAME`, `--name NAME`: prefix the display with a name.
- `-f`, `--force`: display progress when standard error is not a terminal.
- `-c`, `--cursor`: use cursor positioning for display updates.
- `-L RATE`, `--rate-limit RATE`: limit transfer rate in bytes per second.
- `-B SIZE`, `--buffer-size SIZE`: set read buffer size (default `64K`).
- `-E`, `--skip-errors`: continue after read errors.
- `-S`, `--stop-at-size`: stop after the specified size is read.
- `-P FILE`, `--pidfile FILE`: write the process ID to a file.
- `-h`, `--help`: show help; `-V`, `--version`: show version.