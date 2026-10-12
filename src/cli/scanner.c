#include "cli/scanner.h"
#include "primitive/string.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** The host feeds finite ASCII spans into caller-owned backing. Scanner has
 * no blocking stdin, OS object, allocation while feeding, or hidden thread.
 * Cold reserve copies remaining input to larger backing; token extraction
 * allocates R2 strings only after a delimiter or explicit EOF is available. */
;;OVERVIEW
/** ========================================================================
 * CLASS: Scanner (cli/scanner.c)
 * STRUCT FIELDS (Mirroring cli/scanner.h)
 * char *bytes                 borrowed input backing
 * size_t capacity             physical admitted extent
 * size_t length               initialized input bytes
 * size_t position             consumed prefix
 * size_t refusalCount         epoch capacity refusals
 * bool ended                  explicit caller EOF
 * FUNCTION REGISTRY (exports)
 * Constructors: Scanner Scanner_0(void)
 *   Scanner Scanner_2(char *bytes, size_t capacity)
 * Admission: bool Scanner_feed(Scanner *self, const char *bytes, size_t length,
 *     bool ended)
 *   bool Scanner_reserve(Scanner *self, char *bytes, size_t capacity)
 * Queries: bool Scanner_hasNextLine(const Scanner *self)
 *   bool Scanner_isEnded(const Scanner *self)
 *   const char *Scanner_getBytes(const Scanner *self)
 *   size_t Scanner_getLength(const Scanner *self)
 *   size_t Scanner_getCapacity(const Scanner *self)
 *   size_t Scanner_getPosition(const Scanner *self)
 *   size_t Scanner_exhaustionCount(const Scanner *self)
 * Extraction: uint8_t *Scanner_nextLine(Scanner *self)
 *   uint8_t *Scanner_nextWord(Scanner *self)
 * Lifecycle: void Scanner_reset(Scanner *self) retains backing, clears epoch
 * Projections: bool Scanner_toString(const Scanner *self, char *dest,
 *     size_t cap, bool *outTruncated)
 *   bool Scanner_toStringStruct(const Scanner *self, char *dest,
 *     size_t cap, bool *outTruncated)
 * PRIVATE BEHAVIOR: wordSpace ASCII token delimiter; take allocates exact token
 * and advances position only after success. No caller span retained by feed.
 * CONTRACT: single externally serialized owner; disjoint live spans for reserve
 * and feed. ASCII admitted before mutation; NUL/ESC/control/non-ASCII reject.
 * CR is whitespace and stripped only immediately before LF. Empty line yields
 * an empty string, unlike pending/EOF. Buffer never silently truncates. reserve
 * supports cold growth, reset supports recovery; no permanent initial ceiling.
 * ======================================================================== */
Scanner Scanner_0(void) { return (Scanner) {0}; }
Scanner Scanner_2(char *bytes, size_t capacity) {
    if (bytes == nullptr && capacity != 0) {
        THROW("Scanner construct: null backing");
        return Scanner_0();
    }
    return (Scanner) {.bytes = bytes, .capacity = capacity};
}
bool Scanner_feed(Scanner *self, const char *bytes, size_t length, bool ended) {
    if (self == nullptr || (*self).ended || (bytes == nullptr && length != 0)) {
        THROW("Scanner feed: invalid span or ended");
        return false;
    }
    if (length > (*self).capacity - (*self).length) {
        if ((*self).refusalCount == 0)
            THROW("Scanner exhausted: request %zu bytes, used %zu/%zu", length, (*self).length, (*self).capacity);
        if ((*self).refusalCount != SIZE_MAX)
            ++(*self).refusalCount;
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char) bytes[i];
        if (c == 0 || c >= 127 || (c < 32 && c != '\n' && c != '\r' && c != '\t')) {
            THROW("Scanner feed: invalid ASCII byte at %zu", i);
            return false;
        }
    }
    if (length != 0)
        memcpy((*self).bytes + (*self).length, bytes, length);
    (*self).length += length;
    (*self).ended = ended;
    return true;
}
bool Scanner_reserve(Scanner *self, char *bytes, size_t capacity) {
    if (self == nullptr || bytes == nullptr || capacity < (*self).capacity
        || (bytes == (*self).bytes && capacity != (*self).capacity)) {
        THROW("Scanner reserve: invalid backing");
        return false;
    }
    size_t pending = (*self).length - (*self).position;
    if (pending != 0)
        memmove(bytes, (*self).bytes + (*self).position, pending);
    (*self).bytes = bytes;
    (*self).capacity = capacity;
    (*self).length = pending;
    (*self).position = 0;
    return true;
}
bool Scanner_hasNextLine(const Scanner *self) {
    if (self == nullptr || (*self).position == (*self).length)
        return false;
    return (*self).ended || memchr((*self).bytes + (*self).position, '\n', (*self).length - (*self).position) != nullptr;
}
static uint8_t *take(Scanner *self, size_t start, size_t end, size_t next) {
    if (end - start >= UINT32_MAX) {
        THROW("Scanner take: string length overflow");
        return nullptr;
    }
    uint8_t *result = string_allocateBytes((const uint8_t*) (*self).bytes + start, end - start);
    if (result != nullptr)
        (*self).position = next;
    return result;
}
uint8_t *Scanner_nextLine(Scanner *self) {
    if (self == nullptr) {
        THROW("Scanner nextLine: null scanner");
        return nullptr;
    }
    if (!Scanner_hasNextLine(self))
        return nullptr;
    size_t start = (*self).position;
    size_t end = start;
    while (end < (*self).length && (*self).bytes[end] != '\n')
        ++end;
    size_t next = end < (*self).length ? end + 1 : end;
    size_t stop = end;
    if (stop > start && (*self).bytes[stop - 1] == '\r')
        --stop;
    return take(self, start, stop, next);
}
static bool wordSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
uint8_t *Scanner_nextWord(Scanner *self) {
    if (self == nullptr) {
        THROW("Scanner nextWord: null scanner");
        return nullptr;
    }
    size_t start = (*self).position;
    while (start < (*self).length && wordSpace((*self).bytes[start]))
        ++start;
    size_t end = start;
    while (end < (*self).length && !wordSpace((*self).bytes[end]))
        ++end;
    if (start == end || (end == (*self).length && !(*self).ended))
        return nullptr;
    return take(self, start, end, end);
}
void Scanner_reset(Scanner *self) {
    if (self == nullptr)
        return;
    (*self).length = 0;
    (*self).position = 0;
    (*self).refusalCount = 0;
    (*self).ended = false;
}
size_t Scanner_getLength(const Scanner *self) { return self ? (*self).length : 0; }
size_t Scanner_getCapacity(const Scanner *self) { return self ? (*self).capacity : 0; }
size_t Scanner_getPosition(const Scanner *self) { return self ? (*self).position : 0; }
bool Scanner_isEnded(const Scanner *self) { return self && (*self).ended; }
size_t Scanner_exhaustionCount(const Scanner *self) { return self ? (*self).refusalCount : 0; }
const char *Scanner_getBytes(const Scanner *self) { return self ? (*self).bytes : nullptr; }
bool Scanner_toString(const Scanner *self, char *dest, size_t cap, bool *outTruncated) {
    return Scanner_toStringStruct(self, dest, cap, outTruncated);
}
bool Scanner_toStringStruct(const Scanner *self, char *dest, size_t cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("Scanner projection: missing destination");
        return false;
    }
    int n = self ? snprintf(dest, cap, "Scanner{bytes=<borrowed>,capacity=%zu,length=%zu,position=%zu,refusalCount=%zu,ended=%d}",
        (*self).capacity, (*self).length, (*self).position, (*self).refusalCount, (*self).ended)
        : snprintf(dest, cap, "nullptr");
    bool cut = n < 0 || (size_t) n >= cap;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    if (cut)
        THROW("Scanner projection: truncated at %zu bytes", cap);
    return !cut;
}
