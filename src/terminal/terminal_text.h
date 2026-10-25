#ifndef DETERMINANT_TERMINAL_TEXT_H
#define DETERMINANT_TERMINAL_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"
#include "c23/constructor.h"

// terminal/terminal_text.h — the addressable LEAF.
//
// One of the two components a terminal interface is made of. Text is a leaf: a
// borrowed string drawn on one row of the region its parent resolved for it, with
// its own pen, optional bracket glyphs and an alignment. It never holds children.
//
// Text is ADDRESSABLE on purpose. Anonymous glyphs can be drawn straight into a
// canvas, but nothing can hover them, recolour them or click them — so anything
// the interface may need to talk to later is an object with a name (a pointer the
// application keeps), not a string dumped into a buffer. TerminalButton is the
// same leaf with interaction bolted on.
//
// Italic note: the cell style vocabulary has no italic flag, exactly as the
// terminal host does not. Bold, dim, underline, reverse and the semantic accents
// are the real flags; a caller wanting "italic" uses underline, which is what the
// device backends can actually render.
//
// The pen handed to paint() is the PARENT's pen, so a text inherits the colours
// of the panel it sits on; a colour the text sets for itself wins, and styles
// combine. That is what lets a panel say "everything on me is grey" once.
//
// Ownership: the text owns no memory beyond its own record — the glyph string is
// borrowed and must outlive it. The panel that owns the text frees it.

typedef enum TerminalTextAlign {
    TERMINAL_TEXT_ALIGN_LEFT = 0,
    TERMINAL_TEXT_ALIGN_CENTER,
    TERMINAL_TEXT_ALIGN_RIGHT
} TerminalTextAlign;

typedef struct TerminalText {
    const char *glyphs;      // borrowed UTF-8, must outlive the text
    size_t length;           // bytes in `glyphs`
    TerminalPen pen;         // this text's own style/foreground/background
    uint32_t bracketLeft;    // codepoint drawn before the text (0 = none)
    uint32_t bracketRight;   // codepoint drawn after the text (0 = none)
    TerminalTextAlign align;  // placement inside the resolved region
} TerminalText;

TerminalText *TerminalText_0(void);
TerminalText *TerminalText_1(const char *glyphs);
TerminalText *TerminalText_2(const char *glyphs, size_t length);
#define TerminalText(...) CONSTRUCTOR_DISPATCH(TerminalText, __VA_ARGS__)
#define TerminalText_zero() TerminalText_0()

// Frees the text and its record. The borrowed glyph string is never freed.
void TerminalText_free(TerminalText *self);

bool TerminalText_setText(TerminalText *self, const char *glyphs, size_t length);
const char *TerminalText_getText(const TerminalText *self);
size_t TerminalText_getLength(const TerminalText *self);
bool TerminalText_setPen(TerminalText *self, TerminalPen pen);
TerminalPen TerminalText_getPen(const TerminalText *self);
bool TerminalText_setStyle(TerminalText *self, uint8_t style);
uint8_t TerminalText_getStyle(const TerminalText *self);
bool TerminalText_setForegroundColor(TerminalText *self, uint32_t color);
uint32_t TerminalText_getForegroundColor(const TerminalText *self);
bool TerminalText_setBackgroundColor(TerminalText *self, uint32_t color);
uint32_t TerminalText_getBackgroundColor(const TerminalText *self);
// Bracket glyphs are CODEPOINTS, so box-drawing and CJK brackets both work.
// Setting both to 0 removes them.
bool TerminalText_setBrackets(TerminalText *self, uint32_t left, uint32_t right);
uint32_t TerminalText_getBracketLeft(const TerminalText *self);
uint32_t TerminalText_getBracketRight(const TerminalText *self);
bool TerminalText_setAlign(TerminalText *self, TerminalTextAlign align);
TerminalTextAlign TerminalText_getAlign(const TerminalText *self);

// Cells this text occupies on one row, brackets included. Wide glyphs count two,
// which is why this is measured rather than a byte count.
size_t TerminalText_measure(const TerminalText *self);

// Draw on row 0 of `region`, inheriting `parentPen` where this text sets nothing.
// Clipped to the region; a null text or invalid region is a silent no-op.
void TerminalText_paint(const TerminalText *self, TerminalCanvas *region, TerminalPen parentPen);

TerminalProjection TerminalText_toString(const TerminalText *self);
TerminalProjection TerminalText_toStringStruct(const TerminalText *self);

#endif
