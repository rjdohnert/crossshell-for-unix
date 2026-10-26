unset KSH_PARAMETER_DEFAULT_EXPORT_TEST
print "${KSH_PARAMETER_DEFAULT_EXPORT_TEST=secret-marker}"
cmd.exe /c "if defined KSH_PARAMETER_DEFAULT_EXPORT_TEST (exit /b 1) else (exit /b 0)"
print "child:$?"
