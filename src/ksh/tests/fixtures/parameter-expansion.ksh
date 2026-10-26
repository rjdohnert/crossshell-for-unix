value=alpha-beta
print "${value#alpha-}|${value%-beta}|${value/alpha/omega}"
print "${UNSET:-fallback}"
unset UNSET EMPTY
EMPTY=""
print "${UNSET-default}|${UNSET:-colon}|${EMPTY-default}|${EMPTY:-colon}"
print "${UNSET+set}|${UNSET:+set}|${EMPTY+set}|${EMPTY:+set}"
