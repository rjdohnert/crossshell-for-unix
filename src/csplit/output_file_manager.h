#ifndef CSPLIT_OUTPUT_FILE_MANAGER_H
#define CSPLIT_OUTPUT_FILE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct OutputFileManager OutputFileManager;

/* Validate filename prefixes and the single numeric conversion in a suffix format. */
bool OutputFileManager_validate_prefix(const char* prefix);
bool OutputFileManager_validate_suffix_format(const char* format);

/* Create the output writer; NULL indicates invalid configuration or allocation failure. */
OutputFileManager* OutputFileManager_create(const char* prefix, int digits,
                                            const char* suffix_format, bool keep_files,
                                            bool quiet, bool elide_empty);

/* Close the current piece and create the next numbered output file. */
bool OutputFileManager_open_next_file(OutputFileManager* manager);

/* Write one input line to the current output piece. */
bool OutputFileManager_write_line(OutputFileManager* manager,
                                  const char* line, size_t length);

/* Flush and close the current output piece, reporting its byte count if enabled. */
bool OutputFileManager_close_current(OutputFileManager* manager);

/* Remove files created by this run unless keep-files mode is enabled. */
void OutputFileManager_rollback(OutputFileManager* manager);

/* Close any remaining output and release the manager. */
void OutputFileManager_destroy(OutputFileManager* manager);

#endif
