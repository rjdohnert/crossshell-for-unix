# htop

## What it does

Displays a full-screen interactive process monitor for Windows with per-core CPU usage, memory and pagefile meters, tree view, search, filtering, tagging, setup controls, and process actions.

## Interactive keys

- `F1` / `h`: help
- `F2` / `S`: setup
- `F3` / `/`: search
- `F4` / `\\`: filter
- `F5` / `t`: toggle tree view
- `F6`: change sort field
- `F7`: lower priority
- `F8`: raise priority
- `F9` / `k`: signal/kill menu
- `F10` / `q`: quit
- `Space`: tag or untag the selected process
- `U`: clear all tags
- `u`: user filter
- Arrow keys, Page Up, Page Down, Home, End: navigate the table
- Mouse click and wheel: select and scroll

## Notes

- This implementation is a single-file Windows-native C++17 utility.
- It uses native NT APIs for telemetry and process control.

## UNIX origin

This is an htop-style process monitor adapted for Windows rather than a direct portable clone of a specific Unix release in this repository.
