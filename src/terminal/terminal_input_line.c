#include "terminal/terminal_input_line.h"
#include "c23/constructor.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** A generic single-line input over a caller-owned buffer: a prefix, the typed
 * text, and add/backspace/clear editing. It submits nothing and owns no storage
 * — overload is refused loudly at the buffer capacity. The application reads the
 * buffer on Enter and decides what a submission means. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalInputLine (terminal/terminal_input_line.c)
 * STRUCT FIELDS (Mirroring terminal/terminal_input_line.h)
 * char *buffer             borrowed storage
 * size_t capacity          usable bytes (index [length] stays NUL)
 * size_t length            current text length
 * const char *prefix       prompt prefix (default "> ")
 * TerminalPen prefixPen, textPen  pens
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalInputLine_0(void), TerminalInputLine_2(buffer, capacity)
 * Config: bind/getBuffer/getCapacity/isBound, set/getPrefix, set/getPens
 * Editing: bool add(self, ch), bool backspace(self), void clear(self)
 * Queries: size_t TerminalInputLine_length(self)
 * Render: TerminalInputLine_render(self, canvas)
 * Projections: bool TerminalInputLine_toString/toStringStruct(...)
 * CONTRACT: null/unbound operations reject; add refuses (loud, counted by the
 * caller via the false result) at capacity and on non-printable input; render
 * clips and never allocates or logs.
 * ======================================================================== */

TerminalInputLine TerminalInputLine_0(void) {
    TerminalInputLine line = {0};
    line.prefix = TERMINAL_INPUT_PREFIX_DEFAULT;
    line.prefixPen = TerminalPen_1(TERMINAL_STYLE_BOLD);
    line.textPen = TerminalPen_0();
    return line;
}

TerminalInputLine TerminalInputLine_2(char *buffer, size_t capacity) {
    TerminalInputLine line = TerminalInputLine_0();
    if (buffer != nullptr && capacity >= 2) {
        line.buffer = buffer;
        line.capacity = capacity;
        line.buffer[0] = '\0';
    }
    return line;
}

bool TerminalInputLine_bind(TerminalInputLine *self, char *buffer, size_t capacity) {
    if (self == nullptr || buffer == nullptr || capacity < 2) {
        THROW("TerminalInputLine bind: null or too small");
        return false;
    }
    (*self).buffer = buffer;
    (*self).capacity = capacity;
    (*self).length = 0;
    buffer[0] = '\0';
    return true;
}
const char *TerminalInputLine_getBuffer(const TerminalInputLine *self) { return self ? (*self).buffer : nullptr; }
size_t TerminalInputLine_getCapacity(const TerminalInputLine *self) { return self ? (*self).capacity : 0; }
size_t TerminalInputLine_length(const TerminalInputLine *self) { return self ? (*self).length : 0; }
bool TerminalInputLine_isBound(const TerminalInputLine *self) { return self && (*self).buffer != nullptr; }
bool TerminalInputLine_setPrefix(TerminalInputLine *self, const char *prefix) {
    if (self == nullptr || prefix == nullptr) { THROW("TerminalInputLine prefix: null"); return false; }
    (*self).prefix = prefix;
    return true;
}
const char *TerminalInputLine_getPrefix(const TerminalInputLine *self) {
    return (self && (*self).prefix) ? (*self).prefix : TERMINAL_INPUT_PREFIX_DEFAULT;
}
void TerminalInputLine_setPens(TerminalInputLine *self, TerminalPen prefixPen, TerminalPen textPen) {
    if (self != nullptr) { (*self).prefixPen = prefixPen; (*self).textPen = textPen; }
}
void TerminalInputLine_getPens(const TerminalInputLine *self, TerminalPen *outPrefix, TerminalPen *outText) {
    if (outPrefix) *outPrefix = self ? (*self).prefixPen : TerminalPen_0();
    if (outText) *outText = self ? (*self).textPen : TerminalPen_0();
}

bool TerminalInputLine_add(TerminalInputLine *self, int character) {
    if (self == nullptr || (*self).buffer == nullptr || character < 32 || character >= 127) {
        THROW("TerminalInputLine add: unbound or non-printable");
        return false;
    }
    if ((*self).length + 1 >= (*self).capacity) {
        THROW("TerminalInputLine add: capacity refused");
        return false;
    }
    (*self).buffer[(*self).length++] = (char) character;
    (*self).buffer[(*self).length] = '\0';
    return true;
}
bool TerminalInputLine_backspace(TerminalInputLine *self) {
    if (self == nullptr || (*self).buffer == nullptr || (*self).length == 0)
        return false;
    (*self).buffer[--(*self).length] = '\0';
    return true;
}
void TerminalInputLine_clear(TerminalInputLine *self) {
    if (self == nullptr || (*self).buffer == nullptr)
        return;
    (*self).length = 0;
    (*self).buffer[0] = '\0';
}

void TerminalInputLine_render(const TerminalInputLine *self, TerminalCanvas *canvas) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    const char *prefix = TerminalInputLine_getPrefix(self);
    size_t prefixLen = strlen(prefix);
    TerminalCanvas_text(canvas, 0, 0, prefix, prefixLen, (*self).prefixPen);
    if ((*self).buffer != nullptr)
        TerminalCanvas_text(canvas, prefixLen, 0, (*self).buffer, (*self).length, (*self).textPen);
}

bool TerminalInputLine_toString(const TerminalInputLine *self, char *dest, size_t cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("TerminalInputLine projection: missing destination");
        return false;
    }
    int n = self ? snprintf(dest, cap, "TerminalInputLine{bound=%d,length=%zu,capacity=%zu,prefix=\"%s\"}",
                            (*self).buffer != nullptr, (*self).length, (*self).capacity,
                            TerminalInputLine_getPrefix(self))
                 : snprintf(dest, cap, "nullptr");
    bool cut = n < 0 || (size_t) n >= cap;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    if (cut)
        THROW("TerminalInputLine projection: truncated at %zu bytes", cap);
    return !cut;
}

bool TerminalInputLine_toStringStruct(const TerminalInputLine *self, char *dest, size_t cap, bool *outTruncated) {
    return TerminalInputLine_toString(self, dest, cap, outTruncated);
}
