#ifndef DETERMINANT_MARKDOWN_THEME_H
#define DETERMINANT_MARKDOWN_THEME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/surface/terminal_canvas.h"
#include "ui/markdown/markdown_callout.h"
#include "c23/constructor.h"

// ui/markdown/markdown_theme.h — the role vocabulary and the pen ladder.
//
// A document has STRUCTURE (heading level, callout severity, table header) and
// the theme turns that structure into a pen. Keeping the ladder in one value
// record is what makes `#`, `##`, `###` and `####` distinguishable: they are four
// different entries, not one heading pen with a bold flag, so a level cannot
// silently collapse into another. Severity is the same story — five callout
// entries, one per kind.
//
// Pens are resolved at RENDER time, never baked into rows, so recolouring a live
// document does not reflow it. The theme is a value record: copy it, keep it,
// store it. It owns no memory and borrows its glyph strings.

typedef enum TerminalMarkdownRole {
    TERMINAL_MARKDOWN_BODY = 0,
    TERMINAL_MARKDOWN_HEADING,
    TERMINAL_MARKDOWN_CODE,
    TERMINAL_MARKDOWN_LINK,
    TERMINAL_MARKDOWN_QUOTE,
    TERMINAL_MARKDOWN_RULE,
    TERMINAL_MARKDOWN_MARKER,
    TERMINAL_MARKDOWN_FENCE,
    TERMINAL_MARKDOWN_CALLOUT,
    TERMINAL_MARKDOWN_TABLE_HEADER,
    TERMINAL_MARKDOWN_TABLE_BORDER
} TerminalMarkdownRole;

// The legacy eight-pen record, kept as the stable projection a consumer already
// stores. It names one pen per old role; the theme is the thing that actually
// renders, and this record is how an existing caller talks to it.
typedef struct TerminalMarkdownStyle {
    TerminalPen body;
    TerminalPen heading;
    TerminalPen code;
    TerminalPen link;
    TerminalPen quote;
    TerminalPen rule;
    TerminalPen marker;
    // The fence pen carries BOTH the code text and the block's background band.
    // Give it a translucent bg and the whole row shades over the host backdrop;
    // leave bg unset and the block is invisible.
    TerminalPen fence;
} TerminalMarkdownStyle;

typedef struct TerminalMarkdownTheme {
    TerminalPen heading[6];    // h1..h6 — deliberately six distinct pens
    TerminalPen body;
    TerminalPen marker;        // list bullets, task boxes, image glyph
    TerminalPen code;          // inline code
    TerminalPen link;
    TerminalPen linkHover;     // stronger decoration under the pointer
    TerminalPen quote;
    TerminalPen rule;
    TerminalPen fence;         // code-block text AND the translucent band
    TerminalPen callout[5];    // NOTE, TIP, IMPORTANT, WARNING, CAUTION
    TerminalPen tableHeader;
    TerminalPen tableBorder;
    TerminalPen copy;          // the copy affordance on a fence's first row
    TerminalPen copied;        // the check that replaces it
    const char *bar;           // callout gutter glyph, borrowed
    const char *copyGlyph;     // "⧉"
    const char *copiedGlyph;   // "✓"
} TerminalMarkdownTheme;

// The default ladder: warm accent for h1, cool accent for h2, then weight and
// muting carry the falloff. Every level differs in colour, and no two levels
// share both colour and weight.
TerminalMarkdownTheme TerminalMarkdownTheme_0(void);
#define TerminalMarkdownTheme(...) CONSTRUCTOR_DISPATCH(TerminalMarkdownTheme, __VA_ARGS__)
#define TerminalMarkdownTheme_zero() TerminalMarkdownTheme_0()

// Resolve the pen for one span. `variant` carries the heading level (1..6) for a
// HEADING role and the callout kind for a CALLOUT role; it is ignored elsewhere.
// `hovered` selects linkHover for a link span. The returned pen is the theme's
// base pen — the span's own BOLD/ITALIC/UNDERLINE flags are ORed by the caller,
// so emphasis composes without a per-combination pen.
TerminalPen TerminalMarkdownTheme_pen(const TerminalMarkdownTheme *self,
                                      TerminalMarkdownRole role, uint8_t variant,
                                      bool hovered);

// Projection to and from the legacy eight-pen style record. `fromStyle` copies
// each named pen into its theme slot and gives all six heading levels that one
// heading pen (the caller asked for one colour); `style` reads h1 back. This is
// how the existing setStyle/getStyle contract keeps working beside the ladder.
TerminalMarkdownTheme TerminalMarkdownTheme_fromStyle(TerminalMarkdownStyle style);
TerminalMarkdownStyle TerminalMarkdownTheme_style(const TerminalMarkdownTheme *self);

#endif
