value=outer
function example {
    typeset value=inner
    return 7
}
example
print "$value:$?"
