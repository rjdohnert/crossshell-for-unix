cmd.exe /c "echo(OUT&echo(ERR 1>&2" 2>&1 >stdout-only.txt
cmd.exe /c type stdout-only.txt
cmd.exe /c "echo(OUT&echo(ERR 1>&2" >combined.txt 2>&1
cmd.exe /c type combined.txt
