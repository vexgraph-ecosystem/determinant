#ifndef DETERMINANT_LOGCOMMANDS_H
#define DETERMINANT_LOGCOMMANDS_H
#include "cli/output.h"
// Cold read-only R2 LogParser adapter. All text goes to an explicit sink.
// File must remain stable for the operation; not a hostile-file sandbox.
bool LogCommands_stat(const char *path, DeterminantOutputFn output, void *hostContext);
// limit < 0 means all records. Sink failure returns false; already delivered
// prefix cannot roll back. The underlying R2 parser cannot cancel mid-read.
bool LogCommands_cat(const char *path, int limit, DeterminantOutputFn output, void *hostContext);
#endif
