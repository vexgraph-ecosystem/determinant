#ifndef DETERMINANT_TERMINAL_MARKDOWN_CALLOUT_H
#define DETERMINANT_TERMINAL_MARKDOWN_CALLOUT_H

#include <stdbool.h>
#include <stddef.h>

// ui/markdown/markdown_callout.h — the callout KIND, and the two ways a document
// spells it.
//
// A callout is a quoted aside with a severity: NOTE, TIP, IMPORTANT, WARNING or
// CAUTION. GFM spells it `> [!WARNING]`; a hand-written README often spells the
// same intent `!WARNING!`. Both mean one kind here, so the renderer has one bar,
// one label and one pen per severity instead of a literal `! WARNING !` made of
// punctuation. This owner parses the marker and names it; where the bar sits and
// how wide the indent is belong to the view, and which pen a kind uses belongs to
// the theme.

#define TERMINAL_MARKDOWN_CALLOUT_BAR "▌"

typedef enum TerminalMarkdownCalloutKind {
    TERMINAL_MARKDOWN_CALLOUT_NONE = 0,
    TERMINAL_MARKDOWN_CALLOUT_NOTE,
    TERMINAL_MARKDOWN_CALLOUT_TIP,
    TERMINAL_MARKDOWN_CALLOUT_IMPORTANT,
    TERMINAL_MARKDOWN_CALLOUT_WARNING,
    TERMINAL_MARKDOWN_CALLOUT_CAUTION,
    TERMINAL_MARKDOWN_CALLOUT_KIND_COUNT
} TerminalMarkdownCalloutKind;

// VALUE RECORD: TerminalMarkdownCallout (no behaviour).
// `valid` means the span begins with a recognised marker. `contentStart` is the
// byte just past the marker and one optional space, so the caller can render the
// title as body text. `titleStart/titleLength` are the same slice, named for the
// label row.
typedef struct TerminalMarkdownCallout {
    bool valid;
    TerminalMarkdownCalloutKind kind;
    size_t contentStart;
    size_t titleStart;
    size_t titleLength;
} TerminalMarkdownCallout;

// Parse a marker at the START of the span: `[!KIND]...` or `!KIND!...`, case
// insensitive. An unknown kind is not a callout, so a stray `[!x]` stays literal.
TerminalMarkdownCallout TerminalMarkdownCallout_parse(const char *text, size_t length);

// Upper-case label for the kind ("WARNING"), or "" for NONE.
const char *TerminalMarkdownCallout_name(TerminalMarkdownCalloutKind kind);

// The gutter glyph every kind shares; severity is colour and label, not shape.
const char *TerminalMarkdownCallout_bar(void);

// Index into the theme's callout pen table: 0 for NOTE ... 4 for CAUTION,
// TERMINAL_MARKDOWN_CALLOUT_KIND_COUNT when the kind is NONE.
size_t TerminalMarkdownCallout_penIndex(TerminalMarkdownCalloutKind kind);

#endif
