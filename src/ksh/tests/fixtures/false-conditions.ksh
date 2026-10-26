if test -n ""
then
    print unexpected-if
else
    print false-branch
fi
while test -n ""
do
    print unexpected-while
done
print after
