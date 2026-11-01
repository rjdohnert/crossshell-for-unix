#ifndef CONEXEC_FILE_SNAPSHOT_H
#define CONEXEC_FILE_SNAPSHOT_H

#include "conexec.h"

struct FileSnapshot {
    fs::file_time_type lastWrite;
    uintmax_t size;
};

#endif // CONEXEC_FILE_SNAPSHOT_H
