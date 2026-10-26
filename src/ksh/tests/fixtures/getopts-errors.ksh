getopts ":b:" opt -b
print "missing:$opt:$OPTARG:$OPTIND"
OPTIND=1
getopts ":ab:" opt -x
print "unknown:$opt:$OPTARG:$OPTIND"
