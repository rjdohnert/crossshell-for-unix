# groupmod

## What it does
Modifies an existing local group definition on the Windows SAM or Active Directory database.

This command is Solaris-compatible in interface and maps group updates to native Win32 security operations.

## Usage
`groupmod [options] GROUP`

## Options
- `-g`, `--gid GID`: assign new numerical Group Identifier (GID/RID). GID must be a non-negative decimal integer.
- `-o`, `--non-unique`: allow assignment of a duplicate (non-unique) GID; requires `-g`.
- `-n`, `--new-name NEW_NAME`: rename the group to `NEW_NAME`.
- `-c`, `--comment COMMENT`: set group description comment.
- `-A`, `--attributes KEY=VAL`: set extended project/attribute key-value metadata.
- `-h`, `--help`: display help and exit.
- `-v`, `--version`: display version and exit.

## Windows Mapping Notes
- Group renames update the local SAM alias object.
- Unix-style GIDs map directly to Security Relative Identifiers (RIDs).

## Exit Codes
- `0`: success
- `2`: invalid command syntax
- `3`: invalid argument supplied to option
- `4`: GID is not unique (when `-o` is not specified)
- `9`: group does not exist, or new group name already exists
- `10`: cannot update system SAM/Security database
- `28`: permission denied (must run as Administrator)

## Examples
```powershell
groupmod -n dev_team developers
groupmod -c "Database Operations Team" dba
groupmod -g 1010 engineers
groupmod -g 1010 -o qa_team
```

## UNIX Origin
Solaris `groupmod` style group-management utility.
