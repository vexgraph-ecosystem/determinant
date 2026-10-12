#include "cli/commandparser.h"
#include "primitive/string.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <string.h>

;;DEFINITION
/** R3 cold whitespace tokenization over a caller-provided span. Complete byte
 * admission precedes allocation; each successful token is a real R2 string.
 * Partial construction releases every initialized token/directory on failure.
 * There is no shell syntax, stdin, execution or terminal output. */
;;OVERVIEW
/** ========================================================================
 * MODULE: CommandParser (cli/commandparser.c)
 * STATE: none; no retained caller span.
 * FUNCTION REGISTRY (exports)
 * Command *CommandParser_parse(const char *line)
 * Command *CommandParser_parseBytes(const char *line, size_t length)
 * PRIVATE HELPERS: bool space(unsigned char c), ASCII whitespace predicate.
 * CONTRACT: NUL form requires a live terminated string; span form admits at
 * most UINT32_MAX-1 bytes (R2 string length bound), rejects NUL/non-ASCII and
 * non-whitespace controls. Empty/blank returns null quietly. Allocation failure
 * is reported/counted by R2. Token bytes and pointer storage are all owned;
 * free returned Command before Memory teardown. Caller externally serializes.
 * ======================================================================== */
static bool space(unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
Command *CommandParser_parse(const char *line) {
    if (line == nullptr) {
        THROW("CommandParser parse: null line");
        return nullptr;
    }
    return CommandParser_parseBytes(line, strlen(line));
}
Command *CommandParser_parseBytes(const char *line, size_t length) {
    if ((line == nullptr && length != 0) || length >= UINT32_MAX) {
        THROW("CommandParser parse: null span or length overflow");
        return nullptr;
    }
    size_t tokens = 0;
    bool inToken = false;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char) line[i];
        if (c == 0 || c >= 127 || (c < 32 && !space(c))) {
            THROW("CommandParser parse: invalid ASCII byte at %zu", i);
            return nullptr;
        }
        if (!space(c) && !inToken)
            ++tokens;
        inToken = !space(c);
    }
    if (tokens == 0)
        return nullptr;
    if (tokens > UINT32_MAX / sizeof(uint8_t*)) {
        THROW("CommandParser parse: pointer directory overflow");
        return nullptr;
    }
    uint8_t **parts = Memory_alloc(TYPE_BYTE_ARRAY, tokens * sizeof(uint8_t*));
    if (parts == nullptr)
        return nullptr;
    size_t built = 0;
    size_t i = 0;
    while (i < length) {
        while (i < length && space((unsigned char) line[i]))
            ++i;
        if (i == length)
            break;
        size_t start = i;
        while (i < length && !space((unsigned char) line[i]))
            ++i;
        parts[built] = string_allocateBytes((const uint8_t*) line + start, i - start);
        if (parts[built] == nullptr)
            break;
        ++built;
    }
    Command *result = built == tokens ? Command_3(parts[0], parts + 1, tokens - 1) : nullptr;
    if (result == nullptr)
        for (size_t j = 0; j < built; ++j)
            string_free(parts[j]);
    Memory_free(parts);
    return result;
}
