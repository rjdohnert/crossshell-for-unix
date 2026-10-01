# wall

## What it does
Writes a broadcast message to users on the local Windows system or to a specified remote Windows Terminal Services / Remote Desktop server.

## Usage
```text
wall [-h | --help] [-v | --version] [-n | --nobanner] [-p | --popup]
     [-c | --conhost] [-s SERVER | --server SERVER]
     [-f FILE | --file FILE] [MESSAGE]
```

## Options
- `-h`, `--help`: show help text
- `-v`, `--version`: show version information
- `-n`, `--nobanner`: suppress the banner header showing the sender, host, and timestamp
- `-p`, `--popup`: send the message as a Windows popup dialog to active sessions
- `-c`, `--conhost`: open a local console window and display the broadcast body there
- `-s SERVER`, `--server SERVER`: broadcast to a specific remote Windows Terminal Services / Remote Desktop server
- `-f FILE`, `--file FILE`: read the message from a file instead of stdin
- `MESSAGE`: optional inline message text

## Notes
- Messages may be up to 500 characters long.
- If no message source is provided, `wall` reads from standard input.
- The output is formatted as a dynamic box with sender and timestamp information unless `--nobanner` is used.

## UNIX origin
A classic Unix system message utility from BSD and System V releases, adapted here for Windows Terminal Services and console broadcasting.
