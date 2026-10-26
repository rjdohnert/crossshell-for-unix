value="alpha beta"
set -- $value
print "unquoted:$#:$1:$2"
set -- "$value"
print "quoted:$#:$1:$2"
