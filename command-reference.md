# Command Reference

This is a brief overview of the tools in CrossShell for UNIX. For in-depth documentation and examples for each utility, refer to the [docs/](docs/) folder in this repository.

---

## Text, Document, and Transformation Tools

- [`a2pdf`](docs/a2pdf.md) — Converts text or UTF-8 input into PDF or PostScript output.
- [`awk`](docs/awk.md) — Processes text using pattern-based scripting.
- [`base`](docs/base.md) — Encodes or decodes RFC 4648 Base32 and Base64 data.
- [`basename`](docs/basename.md) — Removes directory portions from a path and returns the file name.
- [`bat`](docs/bat.md) — Displays files or standard input with syntax highlighting, line numbers, and grid decorations.
- [`cal`](docs/cal.md) — Displays a calendar.
- [`cat`](docs/cat.md) — Concatenates and prints files.
- [`cmp`](docs/cmp.md) — Compares two files and reports differences.
- [`comm`](docs/comm.md) — Compares sorted files line by line.
- [`conexec`](docs/conexec.md) — Runs commands under selectable Windows AppContainer isolation policies.
- [`csplit`](docs/csplit.md) — Splits files into smaller pieces based on context or pattern matching.
- [`cut`](docs/cut.md) — Extracts sections from each line of input.
- [`detab`](docs/detab.md) — Converts tabs to spaces in files or standard input.
- [`dirname`](docs/dirname.md) — Extracts the directory portion of a path.
- [`dos2unix`](docs/dos2unix.md) — Converts DOS-style CRLF line endings to Unix LF style.
- [`entab`](docs/entab.md) — Converts eligible spaces and tabs back to tabs.
- [`eve`](docs/eve.md) — Edits text in an EVE-inspired Windows console interface with syntax highlighting and command-mode operations.
- [`expr`](docs/expr.md) — Evaluates expressions and prints the result.
- [`fold`](docs/fold.md) — Wraps long lines of text.
- [`grep`](docs/grep.md) — Searches input for matching patterns.
- [`gsar`](docs/gsar.md) — Searches and replaces byte patterns or strings in text and binary files.
- [`head`](docs/head.md) — Prints the beginning of a file or stream.
- [`join`](docs/join.md) — Joins lines of two files on a common field.
- [`less`](docs/less.md) — Views files and output one screen at a time with paging and search.
- [`m4`](docs/m4.md) — Processes macro-based text transformations.
- [`man`](docs/man.md) — Displays manual pages for commands.
- [`nl`](docs/nl.md) — Numbers lines in a file or stream.
- [`od`](docs/od.md) — Dumps files in octal or other formatted representations.
- [`paste`](docs/paste.md) — Merges lines from multiple files.
- [`patch`](docs/patch.md) — Applies changes to files from unified or context patch data.
- [`pbcopy`](docs/pbcopy.md) — Copies text to the Windows clipboard.
- [`pbpaste`](docs/pbpaste.md) — Writes text, formatted data, or copied file paths from the clipboard to standard output.
- [`pr`](docs/pr.md) — Paginates text for printing-style output.
- [`printf`](docs/printf.md) — Formats and prints output.
- [`read`](docs/read.md) — Reads input from standard input or a file.
- [`recode`](docs/recode.md) — Converts text between character encodings.
- [`search`](docs/search.md) — Recursively searches files and directories by name, extension, type, and metadata filters.
- [`sed`](docs/sed.md) — Edits text streams using sed-style scripts and substitution commands.
- [`seq`](docs/seq.md) — Generates sequences of numbers.
- [`split`](docs/split.md) — Splits a file into smaller pieces.
- [`strings`](docs/strings.md) — Extracts printable text sequences from binary files.
- [`tail`](docs/tail.md) — Prints the end of a file or stream.
- [`tee`](docs/tee.md) — Copies input to standard output and one or more files simultaneously.
- [`tr`](docs/tr.md) — Translates or deletes characters in text.
- [`tsort`](docs/tsort.md) — Performs a topological sort of dependency data.
- [`uniq`](docs/uniq.md) — Filters repeated adjacent lines.
- [`wc`](docs/wc.md) — Counts lines, words, and characters.
- [`xargs`](docs/xargs.md) — Builds and executes commands from input arguments.
- [`zgrep`](docs/zgrep.md) — Searches inside `.gz` files using grep semantics.

---

## Development and Build Tools

- [`autotool`](docs/autotool.md) — Bundles native automake, autoconf, libtool-style library orchestration, and build/port helper subcommands.
- [`bc`](docs/bc.md) — Provides an arbitrary-precision calculator for arithmetic expressions.
- [`cbasic`](docs/cbasic.md) — Runs BASIC programs with a lightweight interpreter.
- [`crossforge`](docs/crossforge.md) — Provides a menu-driven console IDE with syntax highlighting, Rust/Go and native toolchain autodetection, WSL/SSH terminals, and sandboxed build/run/debug.
- [`dc`](docs/dc.md) — Evaluates arbitrary-precision reverse-Polish arithmetic with stack, register, macro, radix, and scale operations.
- [`kgdb`](docs/kgdb.md) — Launches or interacts with kernel debugging tools.
- [`lex`](docs/lex.md) — Generates lexical analyzers via an installed flex/lex backend.
- [`make`](docs/make.md) — Reads a Makefile and builds targets by executing recipes, with parallel job and content-hash support.
- [`makedepend`](docs/makedepend.md) — Scans C and C++ source files and appends object-to-header dependency rules to a Makefile.
- [`mkboot`](docs/mkboot.md) — Builds a bootable WinPE ISO from a source directory or WIM and can inject drivers/startup commands before image generation (requires Windows ADK).
- [`vcc`](docs/vcc.md) — Wraps Microsoft `cl.exe` with GCC/POSIX-style flag translation.
- [`yacc`](docs/yacc.md) — Generates C or C++ LALR(1) parsers from Yacc-style grammar files using its built-in generator.

---

## File, Filesystem, and Archive Tools

- [`bdf`](docs/bdf.md) — Reports disk space usage in BSD/HP-UX format.
- [`chattr`](docs/chattr.md) — Changes Windows file attributes using Unix-style shims.
- [`chgrp`](docs/chgrp.md) — Changes a file's group ownership.
- [`chmod`](docs/chmod.md) — Changes file permissions and ACL masks.
- [`chown`](docs/chown.md) — Changes a file's owner and group.
- [`chroot`](docs/chroot.md) — Runs a command in a different root directory.
- [`compress`](docs/compress.md) — Compresses files using classic Unix-style LZW format.
- [`cp`](docs/cp.md) — Copies files and directories.
- [`cpio`](docs/cpio.md) — Archives and extracts files in cpio format.
- [`dd`](docs/dd.md) — Copies and converts data at the byte block level.
- [`diff`](docs/diff.md) — Compares files and shows differences.
- [`diff3`](docs/diff3.md) — Compares three files at once.
- [`dircolors`](docs/dircolors.md) — Configures color settings for directory listings.
- [`dirtree`](docs/dirtree.md) — Visualizes a directory hierarchy as a tree with ANSI color coding, optional file sizes, and extension filtering.
- [`dskctl`](docs/dskctl.md) — Provides a preview-first Windows cleanup utility for temporary and cache-related files.
- [`du`](docs/du.md) — Reports disk usage for files and directories.
- [`fd`](docs/fd.md) — Searches for files and directories by name pattern with ignore rules and path filters.
- [`fdisk`](docs/fdisk.md) — Manages disk partitions and volume metadata.
- [`file`](docs/file.md) — Identifies file types using magic signature headers.
- [`findmnt`](docs/findmnt.md) — Lists mounted filesystems, drive letters, and volume mount points.
- [`fsctl`](docs/fsctl.md) — Performs filesystem control and sparse/reparse point operations.
- [`getfacl`](docs/getfacl.md) — Displays Windows ACLs in a POSIX-like format.
- [`gzip`](docs/gzip.md) — Compresses files into `.gz` format.
- [`install`](docs/install.md) — Copies or moves files and directory trees with filtering, retries, mirroring, and multithreaded operation.
- [`ln`](docs/ln.md) — Creates hard links, symbolic links, and NTFS junctions between files.
- [`locate`](docs/locate.md) — Finds files by name in a filesystem index.
- [`ls`](docs/ls.md) — Lists files and directories, with human-readable, JSON, CSV, and table output modes for direct piping.
- [`lsattr`](docs/lsattr.md) — Lists Windows file attributes using Unix-style flag letters.
- [`lsblk`](docs/lsblk.md) — Lists block-style devices and mounted volumes.
- [`md5sum`](docs/md5sum.md) — Computes MD5 checksums.
- [`mkfifo`](docs/mkfifo.md) — Creates Windows named pipes.
- [`mktemp`](docs/mktemp.md) — Creates secure temporary files or directories.
- [`mount`](docs/mount.md) — Attaches filesystems and volume mappings to the system.
- [`mv`](docs/mv.md) — Moves or renames files and directories.
- [`mvdir`](docs/mvdir.md) — Moves or renames a directory, with cross-volume recursive copy support.
- [`pax`](docs/pax.md) — Archives and extracts files in portable archive interchange format.
- [`pkg`](docs/pkg.md) — Manages packages or bundled resources.
- [`pwd`](docs/pwd.md) — Prints the current working directory.
- [`realpath`](docs/realpath.md) — Resolves file names to normalized absolute Windows paths.
- [`rename`](docs/rename.md) — Renames files by replacing patterns in their base names.
- [`rm`](docs/rm.md) — Removes files and directories.
- [`rsync`](docs/rsync.md) — Synchronizes files and directories between locations.
- [`sar`](docs/sar.md) — Reports system activity in an HP-UX sar-style layout for CPU, memory, disk, paging, queue, network, and per-core metrics.
- [`setfacl`](docs/setfacl.md) — Applies ACL permission entries to Windows files.
- [`sha256sum`](docs/sha256sum.md) — Computes SHA-256 checksums.
- [`stat`](docs/stat.md) — Displays detailed file and directory metadata for one or more paths.
- [`sync`](docs/sync.md) — Flushes pending file or volume writes to physical storage.
- [`touch`](docs/touch.md) — Updates timestamps or creates empty files.
- [`truncate`](docs/truncate.md) — Shrinks, extends, or rounds file lengths to a requested size.
- [`umask`](docs/umask.md) — Displays or sets the file creation mask.
- [`unxz`](docs/unxz.md) — Decompresses `.xz` files through an external xz backend.
- [`unzip`](docs/unzip.md) — Lists or extracts ZIP archive contents.
- [`vhdctl`](docs/vhdctl.md) — Controls or inspects virtual hard disk (VHD/VHDX) operations.
- [`whence`](docs/whence.md) — Resolves command names to shell built-ins or executable paths using PATH/PATHEXT rules.
- [`whereis`](docs/whereis.md) — Locates the binary, source, or manual page for a command.
- [`which`](docs/which.md) — Shows which executable would be run for a command name.
- [`wipe`](docs/wipe.md) — Overwrites files securely to make their contents harder to recover.
- [`xorriso`](docs/xorriso.md) — Builds ISO images in mkisofs-compatible mode, extracts ISO content, and inspects ISO metadata on Windows.
- [`xz`](docs/xz.md) — Compresses files into `.xz` format through an external xz backend.
- [`zcat`](docs/zcat.md) — Prints decompressed `.gz` file content to standard output.
- [`zip`](docs/zip.md) — Creates and updates ZIP archives.

---

## Identity, Accounts, and Access Control

- [`auditctl`](docs/auditctl.md) — Displays and updates selected Windows audit policy classes.
- [`chcon`](docs/chcon.md) — Changes security context labels on files.
- [`getent`](docs/getent.md) — Looks up passwd, group, and host records from local and domain SAM databases.
- [`groupadd`](docs/groupadd.md) — Creates a new local group definition.
- [`groupdel`](docs/groupdel.md) — Deletes an existing local group definition.
- [`groupmod`](docs/groupmod.md) — Modifies an existing local group definition.
- [`groups`](docs/groups.md) — Lists the groups a user belongs to.
- [`id`](docs/id.md) — Shows user and group identity information and SID values.
- [`last`](docs/last.md) — Shows recent logon and logoff activity from Windows event logs.
- [`logname`](docs/logname.md) — Prints the current user's login name.
- [`logout`](docs/logout.md) — Terminates a login session.
- [`lsuser`](docs/lsuser.md) — Lists local user account attributes in AIX-style format.
- [`pinky`](docs/pinky.md) — Displays brief user account information.
- [`runcon`](docs/runcon.md) — Runs a command with a specific security context.
- [`su`](docs/su.md) — Switches to another user account.
- [`sudo`](docs/sudo.md) — Executes a command with elevated administrator privileges.
- [`useradd`](docs/useradd.md) — Creates a new user account.
- [`userdel`](docs/userdel.md) — Removes a user account.
- [`usermod`](docs/usermod.md) — Modifies an existing user account.
- [`w`](docs/w.md) — Shows logged-in users and what each session is doing.
- [`wall`](docs/wall.md) — Sends a broadcast message to all logged-in console sessions.
- [`who`](docs/who.md) — Lists users currently logged into the system.

---

## Process, Shell, and Service Control

- [`busybox`](docs/busybox.md) — Offers a multicall binary interface for small Unix-style utilities.
- [`cron`](docs/cron.md) — Schedules and manages recurring background tasks via Windows Task Scheduler.
- [`env`](docs/env.md) — Prints or modifies environment variables for a command execution.
- [`exec`](docs/exec.md) — Runs another program or command in the current process context.
- [`expire`](docs/expire.md) — Runs a command with a maximum runtime and terminates it if exceeded.
- [`false`](docs/false.md) — Exits with a non-zero failure status.
- [`handles`](docs/handles.md) — Reports per-process handle counts with filtering, sorting, limiting, and structured output.
- [`kill`](docs/kill.md) — Terminates running processes by PID.
- [`killall`](docs/killall.md) — Terminates all matching processes by executable name.
- [`ksh`](docs/ksh.md) — Starts or runs the Korn shell (ksh93-compatible native Win32 shell).
- [`ltrace`](docs/ltrace.md) — Intercepts and logs DLL function calls made by a process using software breakpoints and the Windows debug API.
- [`newshell`](docs/newshell.md) — Launches a new Windows Terminal or conhost session in a chosen directory with optional elevation.
- [`nice`](docs/nice.md) — Runs a command with an adjusted process scheduling priority.
- [`nohup`](docs/nohup.md) — Runs a command detached from console hangups and redirects output to `nohup.out`.
- [`pgrep`](docs/pgrep.md) — Finds process IDs by executable-name pattern.
- [`pkill`](docs/pkill.md) — Terminates processes selected by executable-name pattern.
- [`printenv`](docs/printenv.md) — Prints environment variables.
- [`ps`](docs/ps.md) — Lists process status, PID, session, and resource information.
- [`pskill`](docs/pskill.md) — Terminates processes by PID or name pattern with tree traversal, signal modes, and dry-run preview.
- [`pstree`](docs/pstree.md) — Displays running processes as a tree illustrating parent-child relationships.
- [`pthctl`](docs/pthctl.md) — Manages User and System PATH entries (list, add, remove, check, and clean duplicates).
- [`reboot`](docs/reboot.md) — Restarts the system cleanly.
- [`refresh`](docs/refresh.md) — Sends service refresh control signals with optional restart fallback.
- [`script`](docs/script.md) — Records a command session transcript to a file.
- [`sequence`](docs/sequence.md) — Sorts lines or fields from files or standard input.
- [`sleep`](docs/sleep.md) — Pauses execution for a specified duration with decimal/millisecond precision.
- [`spawn`](docs/spawn.md) — Creates a Windows subprocess in a new console, with optional waiting and I/O redirection.
- [`startsrc`](docs/startsrc.md) — Starts a service using AIX-compatible SRC semantics.
- [`stdbuf`](docs/stdbuf.md) — Adjusts buffering operations for command standard I/O streams.
- [`stop`](docs/stop.md) — Stops one or more processes by PID using signal-style semantics.
- [`stopsrc`](docs/stopsrc.md) — Stops a service using AIX-compatible SRC semantics.
- [`strace`](docs/strace.md) — Traces system calls made by a process.
- [`supervisord`](docs/supervisord.md) — Runs and controls configured background programs with restart policy and service mode.
- [`suspend`](docs/suspend.md) — Suspends or resumes processes by PID.
- [`tcsh`](docs/tcsh.md) — Starts or runs the C shell (tcsh-compatible native shell).
- [`test`](docs/test.md) — Evaluates conditional expressions and returns a status code.
- [`threads`](docs/threads.md) — Reports per-process thread counts with filtering, sorting, minimum-count selection, and structured output.
- [`true`](docs/true.md) — Exits with a zero success status.
- [`typeset`](docs/typeset.md) — Sets shell variable attributes and data types.
- [`ulimit`](docs/ulimit.md) — Reports process resource usage or launches a child under Windows Job Object limits.
- [`watch`](docs/watch.md) — Repeatedly runs a command and refreshes its output in the console.
- [`yes`](docs/yes.md) — Repeatedly writes `y` or a supplied string until interrupted.
- [`zsh`](docs/zsh.md) — Starts or runs the Z shell with prompt customization, job control, arrays, and arithmetic evaluation.

---

## Networking, Remote Access, and Communication

- [`dig`](docs/dig.md) — Queries DNS records with standard dig-style output.
- [`dladm`](docs/dladm.md) — Displays and configures Solaris-style data links, physical adapters, and VLAN or aggregation metadata.
- [`host`](docs/host.md) — Performs forward and reverse DNS lookups.
- [`inetd`](docs/inetd.md) — Runs an inetd-style super-server for configured TCP and UDP services.
- [`ipadm`](docs/ipadm.md) — Displays and configures Solaris-style IP interfaces, address objects, and related protocol properties.
- [`mailx`](docs/mailx.md) — Sends email messages via SMTP.
- [`mtr`](docs/mtr.md) — Continuously traces an IPv4 route and reports per-hop latency and packet-loss statistics.
- [`nc`](docs/nc.md) — Creates TCP/UDP client and listener sockets for relay, scan, and command-pipe workflows (netcat).
- [`netctl`](docs/netctl.md) — Captures and analyzes network traffic with Winsock or optional Npcap backends.
- [`nfsctl`](docs/nfsctl.md) — Controls and diagnoses Windows NFS client/server state, mounts, probes, and registry settings.
- [`rcp`](docs/rcp.md) — Copies files and directories between local and remote hosts using the remote copy protocol.
- [`resolve`](docs/resolve.md) — Queries DNS name servers interactively or in one-shot mode.
- [`ss`](docs/ss.md) — Displays active TCP and UDP socket connection information.
- [`traceroute`](docs/traceroute.md) — Discovers the network path to a host by probing with increasing hop limits.
- [`ufw`](docs/ufw.md) — Manages Windows Defender Firewall rules with UFW-style syntax.
- [`wget`](docs/wget.md) — Downloads files from web and FTP servers.
- [`whois`](docs/whois.md) — Queries domain and IP registration information via WHOIS.

---

## System, Hardware, and Performance

- [`arch`](docs/arch.md) — Reports system processor architecture and platform details.
- [`clock`](docs/clock.md) — Displays or configures clock and time synchronization information.
- [`dmesg`](docs/dmesg.md) — Reads System event logs and prints filtered kernel, driver, and service messages in dmesg format.
- [`driverctl`](docs/driverctl.md) — Enumerates PnP devices and diagnoses driver health, architecture, and filter-stack details.
- [`errpt`](docs/errpt.md) — Displays Windows Event Log entries in an AIX-style summary view.
- [`hostid`](docs/hostid.md) — Displays the numeric host identifier.
- [`htop`](docs/htop.md) — Shows an interactive full-screen process monitor with tree view, search, filters, and process controls.
- [`ioscan`](docs/ioscan.md) — Scans and reports hardware devices in an HP-UX-like inventory view.
- [`iostat`](docs/iostat.md) — Reports disk and partition input/output throughput statistics.
- [`logger`](docs/logger.md) — Writes a message to the console and the Windows Application Event Log.
- [`lsbt`](docs/lsbt.md) — Provides boot configuration, BCD, and EFI information in a lightweight form.
- [`lscfg`](docs/lscfg.md) — Lists installed hardware resources and Vital Product Data (VPD) in AIX-style format.
- [`lsdev`](docs/lsdev.md) — Lists hardware devices with status, driver, and device class details.
- [`lspci`](docs/lspci.md) — Reports PCI device bus and hardware ID information.
- [`lssrc`](docs/lssrc.md) — Lists Windows service status in AIX SRC-style format.
- [`lsusb`](docs/lsusb.md) — Reports USB controllers and connected USB device information.
- [`lsvg`](docs/lsvg.md) — Reports volume groups and logical/physical volume state in AIX-style layout.
- [`lswifi`](docs/lswifi.md) — Lists Wi-Fi adapters and nearby access points with signal strength, channel, and connection status.
- [`machinfo`](docs/machinfo.md) — Displays machine firmware, CPU topology, memory, TPM, and OS summary information in HP-UX format.
- [`meminfo`](docs/meminfo.md) — Reports physical memory, commit charge, kernel pools, and paging-file usage.
- [`nodename`](docs/nodename.md) — Prints the system network node name.
- [`nproc`](docs/nproc.md) — Prints the number of available logical processing units.
- [`prtconf`](docs/prtconf.md) — Prints system configuration summary in an AIX-inspired format.
- [`ps`](docs/ps.md) — Lists process information.
- [`ptime`](docs/ptime.md) — Reports high-resolution process execution timing and elapsed time.
- [`setboot`](docs/setboot.md) — Views and updates UEFI boot parameters and boot-order settings.
- [`swlist`](docs/swlist.md) — Lists installed software from Windows registry sources in HP-UX-style views.
- [`top`](docs/top.md) — Shows live system and process CPU/memory activity.
- [`tty`](docs/tty.md) — Prints the console terminal device name.
- [`uname`](docs/uname.md) — Prints operating system and kernel release information.
- [`uptime`](docs/uptime.md) — Shows how long the Windows system has been running.
- [`vmstat`](docs/vmstat.md) — Reports virtual memory, paging, interrupt, and CPU activity over time.
- [`whereami`](docs/whereami.md) — Reports geographic coordinates and optional address, altitude, accuracy, and network details.

---

## Additional Utilities & Developer Tooling

- [`cmake`](docs/cmake.md) — Configures and builds CMake projects.
- [`hostmap`](docs/hostmap.md) — Displays and manages hostname-to-IP address mappings (`hosts` file).
- [`iconv`](docs/iconv.md) — Converts text streams between character encodings.
- [`netmap`](docs/netmap.md) — Discovers local network interfaces and address mappings.
- [`ninja`](docs/ninja.md) — Runs Ninja build files and targets.
- [`portmap`](docs/portmap.md) — Displays network port mappings and listening services.
- [`purge`](docs/purge.md) — Permanently removes selected files and directories bypassing Recycle Bin.
- [`recycle`](docs/recycle.md) — Moves selected files and directories to the Windows Recycle Bin.
- [`registryctl`](docs/registryctl.md) — Inspects, queries, and manages Windows Registry keys and values.
- [`renice`](docs/renice.md) — Changes the scheduling priority of running processes.
- [`rpm`](docs/rpm.md) — Manages RPM package installation, queries, verification, and removal.
- [`rpmbuild`](docs/rpmbuild.md) — Builds RPM binary and source packages from specification files.
- [`tmux`](docs/tmux.md) — Provides terminal multiplexer sessions, windows, and panes.
- [`ts`](docs/ts.md) — Prefixes standard input lines with timestamps.
