OPTIND=1
getopts "ab:" opt -ab value
print "one:$opt:$OPTARG:$OPTIND"
getopts "ab:" opt -ab value
print "two:$opt:$OPTARG:$OPTIND"
OPTIND=1
getopts "ab:" opt -ab value
print "reset:$opt:$OPTARG:$OPTIND"
