#include "cli/logcommands.h"
#include "io/file.h"
#include "io/logparser.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/what.h"
#include <stdio.h>

;;DEFINITION
/** Read-only R3 presentation of the existing R2 log format, always through a
 * borrowed host sink rather than stdout. Stable-file admission rejects incomplete
 * record extents. Rendering raw integer timestamps avoids the legacy formatter's
 * signed subtraction overflow on hostile timestamp pairs. No OS ownership moves. */
;;OVERVIEW
/** ========================================================================
 * MODULE: LogCommands (cli/logcommands.c)
 * PUBLIC REGISTRY
 * bool LogCommands_stat(const char *path, DeterminantOutputFn output,
 *     void *hostContext)
 * bool LogCommands_cat(const char *path, int limit, DeterminantOutputFn output,
 *     void *hostContext)
 * PRIVATE HELPERS: bool admit(path, output, outBytes); checks format and extent.
 * void catHandler(context,kind,ts,v0,v1,v2,v3,v4); bounded per-record sink.
 * PRIVATE HELPERS: Cat (file-local behaviorless accumulator)
 * int64_t shown             delivered record count
 * int64_t total             parsed record count
 * int limit                 max records (<0 all)
 * bool failed               sticky sink/format failure
 * DeterminantOutputFn output borrowed callback
 * void *hostContext         borrowed opaque sink state
 * CONTRACT: synchronous cold file reads may block, never hot/bounded teardown.
 * Externally serialize and keep file stable. Null/malformed extents reject
 * loudly; output/parse failures return false, preserve no misleading success.
 * Sink receives bounded literal text spans; no raw application printf. Cat
 * stops delivering after failure but parser continues (R2 has no cancel seam).
 * ======================================================================== */
typedef struct Cat {
    int64_t shown;
    int64_t total;
    int limit;
    bool failed;
    DeterminantOutputFn output;
    void *hostContext;
} Cat;
static bool admit(const char *path, DeterminantOutputFn output, int64_t *outBytes) {
    if (path == nullptr || output == nullptr) {
        THROW("LogCommands admission: null path or sink");
        return false;
    }
    if (!LogParser_isLogFile(path)) {
        THROW("LogCommands admission: invalid log");
        return false;
    }
    File *file = File_open(path, FILE_MODE_READ);
    int64_t bytes = file ? File_size(file) : 0;
    if (file != nullptr)
        File_close(file);
    if (bytes < LOGPARSER_HEADER_BYTES || (bytes - LOGPARSER_HEADER_BYTES) % LOGPARSER_RECORD_BYTES != 0) {
        THROW("LogCommands admission: incomplete record extent");
        return false;
    }
    *outBytes = bytes;
    return true;
}
bool LogCommands_stat(const char *path, DeterminantOutputFn output, void *hostContext) {
    int64_t bytes;
    if (!admit(path, output, &bytes))
        return false;
    char text[160];
    int n = snprintf(text, sizeof(text), "log: records=%lld bytes=%lld payload=%lld",
        (long long) ((bytes - LOGPARSER_HEADER_BYTES) / LOGPARSER_RECORD_BYTES),
        (long long) bytes, (long long) (bytes - LOGPARSER_HEADER_BYTES));
    if (n < 0 || (size_t) n >= sizeof(text) || !output(hostContext, text, (size_t) n)) {
        THROW("LogCommands stat: formatting or sink failure");
        return false;
    }
    return true;
}
static void catHandler(void *context, int kind, int64_t ts, int64_t v0, int64_t v1,
                       int64_t v2, int64_t v3, int64_t v4) {
    Cat *cat = context;
    ++(*cat).total;
    if ((*cat).failed || ((*cat).limit >= 0 && (*cat).shown >= (*cat).limit))
        return;
    char text[256]; // Seven int64/32 values + fixed separators fit by construction.
    int n = snprintf(text, sizeof(text), "timestamp=%lld kind=%d values=%lld %lld %lld %lld %lld",
        (long long) ts, kind, (long long) v0, (long long) v1, (long long) v2,
        (long long) v3, (long long) v4);
    if (n < 0 || (size_t) n >= sizeof(text) || !(*cat).output((*cat).hostContext, text, (size_t) n)) {
        (*cat).failed = true;
        return;
    }
    ++(*cat).shown;
}
bool LogCommands_cat(const char *path, int limit, DeterminantOutputFn output, void *hostContext) {
    int64_t bytes;
    if (!admit(path, output, &bytes))
        return false;
    Cat cat = {.limit = limit, .output = output, .hostContext = hostContext};
    int64_t count = LogParser_parse(path, catHandler, &cat);
    if (count != (bytes - LOGPARSER_HEADER_BYTES) / LOGPARSER_RECORD_BYTES || cat.failed) {
        THROW("LogCommands cat: parse or sink failure");
        return false;
    }
    if (cat.shown < cat.total) {
        char text[64];
        int n = snprintf(text, sizeof(text), "(%lld more)", (long long) (cat.total - cat.shown));
        if (n < 0 || (size_t) n >= sizeof(text) || !output(hostContext, text, (size_t) n)) {
            THROW("LogCommands cat: tail sink failure");
            return false;
        }
    }
    return true;
}
