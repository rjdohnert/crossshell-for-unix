# groupadd

## What it does
Creates a new local group definition on the Windows SAM or Active Directory database.

This command is Solaris-compatible in interface and maps group creation to native Win32 security operations.

## Usage
`groupadd [options] GROUP`

## Options
- `-g`, `--gid GID`: assign numerical Group Identifier (GID/RID). GID must be a non-negative decimal integer.
- `-o`, `--non-unique`: allow creation with a duplicate (non-unique) GID; requires `-g`.
- `-c`, `--comment COMMENT`: set description comment for the new local group.
- `-A`, `--attributes KEY=VAL`: set extended project/attribute key-value metadata.
- `-h`, `--help`: display help and exit.
- `-v`, `--version`: display version and exit.

## Windows Mapping Notes
- Unix-style GIDs map directly to Security Relative Identifiers (RIDs).
- Group creation adds a new Local Group object to SAM.

## Exit Codes
- `0`: success
- `2`: invalid command syntax
- `3`: invalid argument supplied to option
- `4`: GID is not unique (when `-o` is not specified)
- `9`: group name already exists
- `10`: cannot update system SAM/Security database
- `28`: permission denied (must run as Administrator)

## Examples
```powershell
groupadd developers
groupadd -g 1005 engineers
groupadd -g 1005 -o qa_team
groupadd -c "Database Administrators" dba
```

## UNIX Origin
Solaris `groupadd` style group-management utility.
