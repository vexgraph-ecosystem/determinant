#ifndef DETERMINANT_MARKDOWN_INLINE_H
#define DETERMINANT_MARKDOWN_INLINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/markdown/markdown_theme.h"

// ui/markdown/markdown_inline.h — the cold inline parser.
//
// Markdown's inline grammar is small and recursive: escapes, code runs, strong
// and emphasis (alone and nested), links with a target, images, autolinks, and
// plain text. The parser turns a borrowed line into a flat run of SPANS — each
// span already carrying its role, its style flags and, for a link, the borrowed
// target — so the layout pass only places text and never re-reads punctuation.
// The syntax markers are consumed here; nothing downstream sees an asterisk that
// meant emphasis.
//
// The span buffer is caller-owned and fixed: the parser never allocates, and a
// document too rich for the buffer reports `complete = false` rather than
// silently dropping text. This is cold-path parsing under the Cold-Only
// Reflection Law's spirit: resolve once at layout, hold the result.

#define MARKDOWN_INLINE_SPANS 96u

// SLOT RECORD: MarkdownSpan (output of the parser).
// `text`/`length` are the display bytes. `role` picks the base pen. `style`
// carries emphasis flags (BOLD/ITALIC/UNDERLINE). For a link every span of the
// label carries the same borrowed `link`/`linkLength`, so a click anywhere on the
// label resolves the target without a side table. `link` is nullptr otherwise.
typedef struct MarkdownSpan {
    const char *text;
    size_t length;
    TerminalMarkdownRole role;
    uint16_t style;
    const char *link;
    size_t linkLength;
} MarkdownSpan;

// Parse `text` into `spans`. `base` is the role plain text inherits and `style`
// the flags already in force (a link label passes its link style down).
// Returns the number of spans written; sets `complete` to false when the buffer
// filled or the nesting cap was reached. A null/empty span writes nothing.
size_t TerminalMarkdownInline_parse(const char *text, size_t length, TerminalMarkdownRole base,
                            uint16_t style, MarkdownSpan *spans, size_t capacity,
                            bool *complete);

#endif
