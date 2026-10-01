# rpmbuild

## What it does

Builds binary and source RPM packages from Windows-compatible `.spec` files.

## Usage

```text
rpmbuild -bb <SPEC_FILE>
rpmbuild -ba <SPEC_FILE>
rpmbuild -bl <SPEC_FILE>
```

## Example

```text
rpmbuild -bb dev/samples/windows-hello.spec
```

## Related commands

- `rpm` installs, queries, verifies, and removes built packages.

## UNIX origin

`rpmbuild` is the RPM build tool used by RPM-based Unix and Linux systems.