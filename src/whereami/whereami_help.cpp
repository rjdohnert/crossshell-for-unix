#include "whereami_help.hpp"

const char* const BSD_MANUAL =
R"(NAME
     whereami -- display current geographical coordinates and location

SYNOPSIS
     whereami [-aAdhjnqsvV] [-f format] [-t seconds] [--from provider]
              [--basedir directory] [--statedir directory]

DESCRIPTION
     The whereami utility interrogates available location sensors and network
     interfaces to determine the physical and geographical coordinates of the
     host. It is the location counterpart to whoami(1) and pwd(1).

OPTIONS
     -f format, --format=format
             Specify the output representation. Valid options:
             raw          Decimal degrees: <latitude>,<longitude> (Default)
             sexagesimal  Degrees, minutes, and seconds (DMS) format
             json         Standard JSON formatted object
             human        Multi-line detailed human-readable summary
             address      Civic geographic address (City, Region, Country)

     -j, --json
             Shorthand for --format=json.

     -s, --sexagesimal
             Shorthand for --format=sexagesimal.

     -a, --accuracy
             Display estimated horizontal accuracy radius in meters.

     -A, --altitude
             Display altitude above sea level (if supported by sensor).

     -d, --address
             Shorthand for --format=address.

     -n, --network
             Include network telemetry (Public IP, Local IP, ISP, Hostname).

     -t seconds, --timeout=seconds
             Set the provider response deadline in seconds (Default: 5).

     --from=provider
             Force a specific location discovery provider:
             auto     Probe local sensor first, fall back to network (Default)
             sensor   Query Windows Location Sensor API exclusively
             network  Query network-based positioning exclusively

     -v, --verbose, --debug
             Print diagnostic operational details during discovery.

     -q, --quiet
             Suppress all diagnostics and warnings; output only the location.

     --basedir=directory
             Specify base directory for configuration files (BSD compat).

     --statedir=directory
             Specify state directory for location records (BSD compat).

     -h, --help
             Display this manual documentation and exit.

     -V, --version
             Display version information and exit.

EXIT STATUS
     0       Location successfully acquired and displayed.
     1       Location could not be determined or permission was denied.
     2       Invalid command-line arguments or configuration error.

EXAMPLES
     Display location in standard decimal coordinates:
           $ whereami
           40.712776,-74.005974

     Display coordinates in sexagesimal DMS notation with accuracy:
           $ whereami -s -a
           40° 42' 46.00" N, 74° 0' 21.51" W [Acc: ±15m]

     Obtain a structured JSON object for automated scripting:
           $ whereami -j -n
)";
