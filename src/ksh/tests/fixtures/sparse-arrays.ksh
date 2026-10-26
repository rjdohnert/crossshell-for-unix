typeset -a items
items[0]=zero
items[2]=two
print "${#items[@]}:${items[0]}:${items[1]}:${items[2]}"
unset items[0]
print "${#items[@]}:${items[2]}"
