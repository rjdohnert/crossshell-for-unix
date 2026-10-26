function f {
    print kept | read value
    print "read:$value"
}
f >out.txt
read result <out.txt
print "file:$result"
