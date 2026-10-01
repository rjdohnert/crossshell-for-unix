# getent

## What it does
Provides a baseline `getent` command that looks up `passwd`, `group`, and `hosts` records using Windows-backed sources.

## Databases
- `passwd`: local user accounts
- `group`: local groups
- `hosts`: DNS host lookup

## Options
- `-h`, `--help`: show help text
- `-V`, `--version`: show version information

## UNIX origin
A name service query utility used by scripts that need database-style account and host lookups.