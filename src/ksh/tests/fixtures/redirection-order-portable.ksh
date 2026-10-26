function emit {
    print OUT
    print -u2 ERR
}
emit 2>&1 >first.txt
read first <first.txt
print "first:$first"
emit >second.txt 2>&1
read second <second.txt
print "second:$second"
