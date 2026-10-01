# dig

Run DNS queries with dig-style output.

## Synopsis

dig [name] [type] [+short]

## Notes

- Supported types: `A`, `AAAA`, `MX`, `NS`, `TXT`, `CNAME`, `PTR`
- `+short` prints only answer data
- `@server` syntax is not supported in this build yet

## Examples

```powershell
dig localhost A
dig localhost AAAA +short
```
