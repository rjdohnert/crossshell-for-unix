typeset -a fruits
fruits[0]=apple
fruits[1]=pear
typeset -A colors
colors[blue]=navy
target=before
typeset -n reference=target
reference=after
print "${fruits[0]}:${fruits[1]}:${colors[blue]}:$target"
