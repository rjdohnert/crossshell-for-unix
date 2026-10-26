function emit_streams {
    cd KSH_MISSING_REDIRECTION_PATH
    print function-out
}

cd KSH_MISSING_REDIRECTION_PATH 2>&1 >builtin-first.txt
cmd.exe /c type builtin-first.txt
cd KSH_MISSING_REDIRECTION_PATH >builtin-combined.txt 2>&1
cmd.exe /c type builtin-combined.txt
emit_streams 2>&1 >function-first.txt
cmd.exe /c type function-first.txt
emit_streams >function-combined.txt 2>&1
cmd.exe /c type function-combined.txt
