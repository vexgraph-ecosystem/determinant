#ifndef DETERMINANT_TERMINAL_INPUT_LINE_H
#define DETERMINANT_TERMINAL_INPUT_LINE_H

#include <stdbool.h>
#include <stddef.h>

#include "terminal/terminal_canvas.h"
#include "c23/constructor.h"

// terminal/terminal_input_line.h — a bare, generic single-line input.
//
// Holds a borrowed character buffer and an editing position, draws a prefix
// (default "> ") and the text. It submits nothing: the caller reads the buffer
// on Enter. Storage is caller-owned, so the widget never allocates and refuses
// overload at the buffer capacity (observed, not silent).

#define TERMINAL_INPUT_PREFIX_DEFAULT "> "

typedef struct TerminalInputLine {
    char *buffer;      // borrowed storage
    size_t capacity;   // usable bytes (a NUL terminator is kept at [length])
    size_t length;
    const char *prefix;   // default "> "
    TerminalPen prefixPen;
    TerminalPen textPen;
} TerminalInputLine;

TerminalInputLine TerminalInputLine_0(void);
TerminalInputLine TerminalInputLine_2(char *buffer, size_t capacity);
#define TerminalInputLine(...) CONSTRUCTOR_DISPATCH(TerminalInputLine, __VA_ARGS__)
#define TerminalInputLine_zero() TerminalInputLine_0()

bool TerminalInputLine_bind(TerminalInputLine *self, char *buffer, size_t capacity);
const char *TerminalInputLine_getBuffer(const TerminalInputLine *self);
size_t TerminalInputLine_getCapacity(const TerminalInputLine *self);
size_t TerminalInputLine_length(const TerminalInputLine *self);
bool TerminalInputLine_isBound(const TerminalInputLine *self);
bool TerminalInputLine_setPrefix(TerminalInputLine *self, const char *prefix);
const char *TerminalInputLine_getPrefix(const TerminalInputLine *self);
void TerminalInputLine_setPens(TerminalInputLine *self, TerminalPen prefixPen, TerminalPen textPen);
void TerminalInputLine_getPens(const TerminalInputLine *self, TerminalPen *outPrefix, TerminalPen *outText);

bool TerminalInputLine_add(TerminalInputLine *self, int character);
bool TerminalInputLine_backspace(TerminalInputLine *self);
void TerminalInputLine_clear(TerminalInputLine *self);

void TerminalInputLine_render(const TerminalInputLine *self, TerminalCanvas *canvas);

bool TerminalInputLine_toString(const TerminalInputLine *self, char *dest, size_t cap, bool *outTruncated);
bool TerminalInputLine_toStringStruct(const TerminalInputLine *self, char *dest, size_t cap, bool *outTruncated);

#endif
