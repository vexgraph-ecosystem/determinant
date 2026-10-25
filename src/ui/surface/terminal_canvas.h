#ifndef DETERMINANT_TERMINAL_CANVAS_H
#define DETERMINANT_TERMINAL_CANVAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "terminal/terminal_color.h"
#include "terminal/terminal_style.h"
#include "ui/surface/terminal_projection.h"
#include "c23/constructor.h"

// ui/surface/terminal_canvas.h — the compositor primitive.
//
// A TerminalCanvas is a RECTANGULAR VIEW over a caller-owned cell GRID: four
// parallel planes that describe one screen cell each —
//   cells   (uint32_t)  the GLYPH as a Unicode codepoint (not a byte!)
//   styles  (uint8_t)   host-neutral attribute flags (DIM/BOLD/REVERSE/…)
//   fg      (uint32_t)  packed 0xRRGGBBAA foreground (TERMINAL_COLOR_DEFAULT = unset)
//   bg      (uint32_t)  packed 0xRRGGBBAA background (TERMINAL_COLOR_DEFAULT = unset)
// Every plane is optional except cells; a null plane draws nothing for that
// dimension. Widgets draw through a region and are clipped to it, so a left rail
// can never scribble into the chat column. Regions compose via TerminalCanvas_sub.
//
// Glyphs are codepoints so the toolkit is UTF-8 capable: `text` decodes UTF-8 and
// stores scalars, a double-width glyph (wcwidth == 2) occupies its cell plus a
// continuation marker (0) in the next, and the host blits with wide cells. The
// canvas owns NO memory and NEVER names a terminal.
//
// Drawing takes a TerminalPen (style + fg + bg), so colour is one argument, not
// four. All draw operations clip and never fault on out-of-range coordinates; a
// draw that lands fully outside the region is a silent no-op (composition, not
// resource exhaustion).

// A brush: attribute flags plus foreground/background colours. Build one with
// TerminalPen_make(...) or start from TerminalPen_0() and set fields.
typedef struct TerminalPen {
    uint8_t style;   // TERMINAL_STYLE_* flags
    uint32_t fg;     // packed 0xRRGGBBAA or TERMINAL_COLOR_DEFAULT
    uint32_t bg;     // packed 0xRRGGBBAA or TERMINAL_COLOR_DEFAULT
} TerminalPen;

TerminalPen TerminalPen_0(void);                                  // no flags, default colours
TerminalPen TerminalPen_1(uint8_t style);
TerminalPen TerminalPen_2(uint8_t style, uint32_t fg);
TerminalPen TerminalPen_3(uint8_t style, uint32_t fg, uint32_t bg);
#define TerminalPen(...) CONSTRUCTOR_DISPATCH(TerminalPen, __VA_ARGS__)
#define TerminalPen_zero() TerminalPen_0()

typedef struct TerminalCanvas {
    uint32_t *cells;      // borrowed row-major glyph CODEPOINTS (gridWidth * gridHeight)
    uint8_t *styles;      // borrowed parallel style plane (nullable)
    uint32_t *fg;         // borrowed parallel foreground plane (nullable)
    uint32_t *bg;         // borrowed parallel background plane (nullable)
    size_t gridWidth;     // full grid columns (indexing stride)
    size_t gridHeight;    // full grid rows
    size_t x;             // this region's absolute left column
    size_t y;             // this region's absolute top row
    size_t width;         // this region's columns (already clipped to the grid)
    size_t height;        // this region's rows (already clipped to the grid)
} TerminalCanvas;

// Bind a whole grid as a canvas covering it.
TerminalCanvas TerminalCanvas_0(void);
TerminalCanvas TerminalCanvas_4(uint32_t *cells, uint8_t *styles, size_t width, size_t height);
TerminalCanvas TerminalCanvas_6(uint32_t *cells, uint8_t *styles, uint32_t *fg, uint32_t *bg,
                                size_t width, size_t height);
#define TerminalCanvas(...) CONSTRUCTOR_DISPATCH(TerminalCanvas, __VA_ARGS__)
#define TerminalCanvas_zero() TerminalCanvas_0()

bool TerminalCanvas_isValid(const TerminalCanvas *self);

// Sub-region in LOCAL coordinates, intersected with the parent region and grid.
TerminalCanvas TerminalCanvas_sub(const TerminalCanvas *self, size_t lx, size_t ly,
                                  size_t w, size_t h);

// --- Primitives (all clip; coordinates are local to this region) ---
// `fill` writes one glyph (default ' '); `text` decodes UTF-8 to codepoints.
void TerminalCanvas_fill(TerminalCanvas *self, uint32_t glyph, TerminalPen pen);
void TerminalCanvas_text(TerminalCanvas *self, size_t lx, size_t ly,
                         const char *text, size_t length, TerminalPen pen);
void TerminalCanvas_hline(TerminalCanvas *self, size_t ly, uint32_t glyph, TerminalPen pen);
void TerminalCanvas_vline(TerminalCanvas *self, size_t lx, uint32_t glyph, TerminalPen pen);
void TerminalCanvas_box(TerminalCanvas *self, TerminalPen pen);

typedef struct TerminalCanvasDecodedScalar {
    uint32_t codepoint;
    size_t bytes;
} TerminalCanvasDecodedScalar;

// Decode one UTF-8 scalar; bytes is 0 on malformed input.
TerminalCanvasDecodedScalar TerminalCanvas_utf8Decode(const char *text, size_t length);

typedef struct TerminalCanvasTextRow {
    char data[1024];
    size_t bytes;
    bool truncated;
} TerminalCanvasTextRow;

// Write ONE codepoint at a local cell (width-aware: a double-width glyph marks
// its continuation cell). Use this for glyph tables; `text` decodes UTF-8.
void TerminalCanvas_glyph(TerminalCanvas *self, size_t lx, size_t ly, uint32_t codepoint, TerminalPen pen);
// Cell width for one codepoint (0 combining, 1, 2 wide) — locale-free.
size_t TerminalCanvas_widthOf(uint32_t codepoint);

// --- Inspection (tests / agents) ---
uint32_t TerminalCanvas_glyphAt(const TerminalCanvas *self, size_t lx, size_t ly);
uint8_t  TerminalCanvas_styleAt(const TerminalCanvas *self, size_t lx, size_t ly);
uint32_t TerminalCanvas_fgAt(const TerminalCanvas *self, size_t lx, size_t ly);
uint32_t TerminalCanvas_bgAt(const TerminalCanvas *self, size_t lx, size_t ly);
size_t   TerminalCanvas_getWidth(const TerminalCanvas *self);
size_t   TerminalCanvas_getHeight(const TerminalCanvas *self);
bool     TerminalCanvas_hasStylePlane(const TerminalCanvas *self);
bool     TerminalCanvas_hasColorPlane(const TerminalCanvas *self);
// Return one region row as bounded UTF-8 text.
TerminalCanvasTextRow TerminalCanvas_copyRow(const TerminalCanvas *self, size_t ly);

TerminalProjection TerminalCanvas_toString(const TerminalCanvas *self);
TerminalProjection TerminalCanvas_toStringStruct(const TerminalCanvas *self);

#endif
