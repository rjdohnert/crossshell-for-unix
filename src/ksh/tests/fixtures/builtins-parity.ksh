integer count=15
float ratio=2.5
typeset -f > /dev/null 2>&1
functions > /dev/null 2>&1
print "count=$count:ratio=$ratio"

total=0
for i in 1 2 3 4 5 6; do
    if [ $i -eq 2 ]; then
        continue
    fi
    if [ $i -eq 5 ]; then
        break
    fi
    total=$((total + i))
done
print "total=$total"

ulimit -n 4096
nofile=$(ulimit -n)
print "nofile=$nofile"

redirect 4>custom_out.txt
print -u4 "hello from fd 4"
redirect 4>&-
cat custom_out.txt
rm custom_out.txt

test_var=set
if test -v test_var && test ! -v non_existent_var; then
    print "test_v:ok"
fi

print -- "print_dash_dash:ok"
print "arith_fix:$(( ~0 )):$(( !0 )):$(( 0x1F + 1 ))"
cat <<EOF
heredoc:ok
EOF
