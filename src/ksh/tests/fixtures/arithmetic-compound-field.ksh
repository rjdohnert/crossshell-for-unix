typeset -T Record_t=(integer count=1)
Record_t record
record.count=4
((record.count+=3))
print "count:$((record.count)):square:$((record.count ** 2))"
