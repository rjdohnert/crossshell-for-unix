unset UNSET EMPTY REQUIRED
EMPTY=""
print "no-colon:${UNSET=assigned}:$UNSET:${EMPTY=ignored}:$EMPTY"
unset UNSET EMPTY
EMPTY=""
print "colon:${UNSET:=assigned}:$UNSET:${EMPTY:=assigned-empty}:$EMPTY"
EMPTY=""
print "empty-question:${EMPTY?not-an-error}"
print "${REQUIRED?missing-required}"
