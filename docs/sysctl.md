# sysctl

Windows-native system information utility with an OpenBSD-style command line,
physical-disk inventory, and local volume-space queries. This is **not** a full
OpenBSD kernel MIB implementation. Windows values and extensions are identified
below; unsupported Unix measurements are not simulated.

Requires Windows and a C++17 compiler with the Windows SDK. The executable is
[`bin/sysctl.exe`](../bin/sysctl.exe).

## Build and test

From the repository root with clang++:

```powershell
clang++ -std=c++17 -O2 src\sysctl\sysctl.cpp -o bin\sysctl.exe -liphlpapi -ladvapi32 -lsetupapi -luuid -lpsapi
clang++ -std=c++17 -O2 src\sysctl\tests.cpp -o src\sysctl\sysctl-tests.exe
.\src\sysctl\sysctl-tests.exe
```

Or from a Visual Studio developer command prompt:

```text
cl /nologo /EHsc /std:c++17 /O2 /MT src\sysctl\sysctl.cpp /Fe:bin\sysctl.exe
cl /nologo /EHsc /std:c++17 src\sysctl\tests.cpp /Fe:src\sysctl\sysctl-tests.exe
src\sysctl\sysctl-tests.exe
```

The source links IP Helper, registry, SetupAPI, UUID, and PSAPI libraries using
MSVC-compatible pragmas. Tests do not change the hostname, disk contents, or other
system settings; writable-node tests use an in-memory setter. Live disk geometry
and volume-space results are checked against Windows APIs.

## Usage

```text
sysctl
sysctl -A
sysctl hw.ncpu hw.physmem
sysctl -n kern.osrelease
sysctl -N kern
sysctl hw.*
sysctl hw.cpuspeed
sysctl kern.boottime
sysctl -n kern.boottime
sysctl hw.diskcount hw.disknames
sysctl hw.disk.PhysicalDrive0
sysctl fs.volume.names
sysctl fs.volume.C.size fs.volume.C.free fs.volume.C.available
sysctl -f settings.conf
```

### Command-line behavior

- Reads print `name=value`; `-n` prints only values; `-N` prints only variable names.
- No names or input file defaults to `-a`: list available scalar values.
- `-A` also prints guidance for table-valued names and notices for known
  unavailable compatibility names. These notices go to standard error, not the
  scalar-value output stream.
- Explicit `-a` or `-A` takes precedence over names and `-f`, matching OpenBSD.
  Without listing flags, the file is processed before command-line names.
- A directory or wildcard query such as `hw` or `hw.*` lists its available scalar children.
- Options precede names and may be grouped. `-f file` and `-ffile` are accepted.
  `--` ends option parsing; the first operand also ends option parsing.
- Batch files accept queries and assignments, blank lines, surrounding whitespace,
  and `#` comments, including trailing comments. Whitespace around `=` is accepted
  as a convenience extension.
- Assignments use `name=value`. The obsolete `-w` flag is accepted as a no-op,
  as on OpenBSD; it does not require assignments.
- Successful assignments print `name: old -> new`; `-n` prints the new value only.
  `-q` overrides `-n` and suppresses successful write output, never errors.
- Unknown names and unsuccessful reads/writes return a nonzero exit status.
  Independent queries and batch lines continue after ordinary query errors.
- Querying a table prints the corresponding reader recommendation rather than
  claiming to return a scalar. `hw.diskstats` points to Windows `typeperf`
  PhysicalDisk counters; raw OpenBSD diskstats structures are not implemented.
- Existing non-OpenBSD extensions remain available: `-d` prints descriptions,
  `-e` selects the existing `=` separator, `-i` ignores unknown names, and `-h`,
  `--help`, or `/?` displays help. `-i` does not ignore unavailable measurements,
  permission errors, or other read/write failures.

## Kernel and memory parameters

| Parameter | Meaning |
|-----------|---------|
| `kern.ostype` | `Windows_NT`, not a claim that the kernel is OpenBSD |
| `kern.osrelease`, `kern.version` | Running Windows release and version |
| `kern.hostname` | Active Windows DNS hostname; the only writable parameter |
| `kern.domainname` | Active Windows DNS domain name; empty when unconfigured |
| `kern.boottime` | Estimated boot time, formatted as a local date; `-n` gives epoch seconds |
| `hw.machine`, `hw.model` | Native architecture and CPU brand reported by Windows |
| `hw.cpuspeed` | CPU frequency in MHz |
| `hw.ncpu`, `hw.ncpuonline` | Active logical CPUs across all Windows processor groups |
| `hw.pagesize`, `hw.byteorder` | System page size and byte ordering |
| `hw.physmem`, `hw.physmem64` | Total physical RAM in bytes |
| `hw.freemem` | Windows extension: available physical RAM in bytes |
| `vm.memory_load` | Windows extension: physical memory load as an integer percentage, 0-100 |
| `vm.commit_limit` | Windows extension: system commit limit in bytes |
| `vm.commit_available` | Windows extension: commit available to this process in bytes |
| `vm.swap_total`, `vm.swap_free` | Windows extensions: actual total and unused pagefile bytes, excluding physical RAM |
| `net.inet.ip.forwarding`, `net.inet6.ip6.forwarding` | Windows IPv4 and IPv6 forwarding state, 0 or 1; read-only |

`vm.loadavg` is unavailable: Windows does not expose the Unix 1/5/15-minute load
averages. `hw.usermem` is also unavailable because Windows available RAM is not
OpenBSD's total non-kernel memory. Direct queries fail with an explanation and an
appropriate Windows alternative. `-a` omits these names; `-A` shows their notices.

Earlier versions mislabeled memory percentage as `vm.loadavg`, available RAM as
`hw.usermem`, and commit-limit counters as `vm.swap_*`. Scripts should use the
explicitly named Windows parameters above. `kern.version` retains a Windows
format rather than fabricating an OpenBSD build string.

`kern.hostname` is the writable node. Changing it requires the appropriate Windows
permissions and a restart to take effect; it does not immediately rename the
running system as OpenBSD does. For example, an elevated invocation can use
`sysctl kern.hostname=WORKSTATION01`. Tests never perform this change.

## Disks and physical storage

`hw.diskcount` counts attached Windows disk devices, and `hw.disknames` is a
comma-separated list such as `PhysicalDrive0,PhysicalDrive2`. Discovery uses
present disk interfaces rather than probing a fixed range of disk numbers.
Disk numbers may have gaps, and virtual disks are included. These are Windows
names, not OpenBSD disk names or disklabel identifiers.

For each returned name, the Windows-only `hw.disk.<name>` subtree provides:

| Suffix | Meaning |
|--------|---------|
| `path` | Device path, e.g. `\\.\PhysicalDrive0` |
| `size` | Physical device capacity in bytes, not filesystem space |
| `sectorsize` | Logical sector size in bytes, not physical-sector alignment |
| `model` | Driver-reported product model |
| `vendor` | Driver-reported vendor |
| `serial` | Driver-reported serial number |
| `revision` | Firmware/product revision |
| `bus` | Bus type such as SATA, NVMe, USB, Virtual, or FileBackedVirtual |
| `removable` | 1 if the storage descriptor marks removable media, otherwise 0 |

An empty descriptor string means that the driver did not supply the optional
field; it is not replaced by an invented vendor, serial, or processor model.
Physical device size may exceed the sizes of its mounted volumes.

The Windows-only `fs.volume` subtree reports **local drive-letter volumes**:

- `fs.volume.count` and `fs.volume.names` enumerate letters such as `C,E,F`.
- `fs.volume.C.path`, `.type`, `.filesystem`, and `.label` identify the volume.
- `fs.volume.C.size` is the filesystem's reported capacity in bytes.
- `fs.volume.C.free` is total free bytes.
- `fs.volume.C.available` is free bytes available to the calling user, accounting
  for quotas.

Mapped network drives and volumes mounted only by GUID or folder path are not
included in this drive-letter inventory. Local removable, optical, and RAM drives
are included. Missing media, disconnected devices, restricted access, and driver
errors are reported explicitly and cause a failed query, not a fabricated zero.
Basic physical-device queries request no read/write access, but device policies
can still require elevation. Storage discovery is lazy: help and unrelated
kernel queries do not depend on being able to enumerate disks.

All disk and volume parameters are read-only. There is no partition editing,
formatting, SMART health test, destructive disk check, or raw OpenBSD table ABI.

## Compatibility references

- [OpenBSD sysctl(8)](https://man.openbsd.org/sysctl.8)
- [OpenBSD sysctl(2)](https://man.openbsd.org/sysctl.2)
- [OpenBSD sysctl.conf(5)](https://man.openbsd.org/sysctl.conf.5)
