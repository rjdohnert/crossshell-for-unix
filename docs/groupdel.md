# groupdel

## What it does
Deletes a local group definition from the Windows SAM or Active Directory database.

This command is Solaris-compatible in interface and maps group deletion to native Win32 security operations.

## Usage
`groupdel [options] GROUP`

## Options
- `-h`, `--help`: display help and exit.
- `-v`, `--version`: display version and exit.

## Windows Protection Notes
Built-in system groups (for example, Administrators, Users, Guests) are protected by the Windows Security Accounts Manager (SAM) and cannot be deleted.

## Exit Codes
- `0`: success
- `2`: invalid command syntax
- `6`: group to delete does not exist
- `10`: cannot update system SAM database, or target is a protected system group
- `28`: permission denied (must run as Administrator)

## Examples
```powershell
groupdel developers
groupdel qa_team
```

## UNIX Origin
Solaris `groupdel` style group-management utility.
