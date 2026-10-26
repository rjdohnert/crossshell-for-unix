target=outer
function mutate {
    typeset target=local
    typeset -n ref=target
    ref=changed
    print "inside:$target"
}
mutate
print "outside:$target"
