# usermod

## What it does
Modifies an existing user account on the local SAM or Active Directory database.

This command is Solaris-compatible in interface and maps user management behavior to native Win32 security operations.

## Usage
`usermod [options] USERNAME`

## Options
- `-c`, `--comment COMMENT`: set the user's GECOS or full-name description field.
- `-d`, `--home-dir DIR`: set the user's home directory path.
- `-m`, `--move-home`: move contents of the current home directory to the new directory set with `-d`.
- `-e`, `--expire-date DATE`: set account expiration date (`YYYY-MM-DD` or `never`).
- `-f`, `--inactive DAYS`: set password inactivity period or expiration flag.
- `-g`, `--gid GROUP`: set the user's primary local group membership.
- `-G`, `--groups G1,G2...`: set secondary local group memberships (comma-separated).
- `-a`, `--append`: append secondary groups from `-G` instead of replacing existing memberships.
- `-l`, `--login NEW_NAME`: rename the user account to `NEW_NAME`.
- `-L`, `--lock`: lock the user account (disable SAM login).
- `-U`, `--unlock`: unlock the user account (enable SAM login).
- `-s`, `--shell SCRIPT_PATH`: set login shell or logon script path.
- `-p`, `--password PASSWORD`: set a new password for the user account.
- `-u`, `--uid UID`: display Security Identifier (SID/RID) mapping info.
- `-h`, `--help`: display help and exit.
- `-v`, `--version`: display version and exit.

## Exit Codes
- `0`: success
- `1`: invalid command syntax
- `2`: invalid argument supplied to option
- `3`: specified user does not exist
- `4`: specified group does not exist
- `6`: specified new login name already exists
- `9`: user is currently logged in
- `10`: cannot update system SAM/Security database
- `12`: cannot move home directory
- `28`: permission denied (must run as Administrator)

## Examples
```powershell
usermod -c "Roberto Dohnert" rjdohnert
usermod -d C:\Users\rjdohnert_new -m rjdohnert
usermod -G Administrators,Users -a rjdohnert
usermod -L rjdohnert
usermod -e 2026-12-31 rjdohnert
usermod -l newusername oldusername
```

## UNIX Origin
Solaris `usermod` style account-management utility.
