#ifndef DETERMINANT_MARKDOWN_TEXT_H
#define DETERMINANT_MARKDOWN_TEXT_H

#include <stdbool.h>
#include <stddef.h>

// ui/markdown/markdown_text.h — terminal-cell measurement over borrowed bytes.
//
// The markdown owners all speak the same units: a byte span is measured in
// CELLS, never in bytes, because a CJK glyph occupies two cells and a combining
// mark occupies none. This is the one place that arithmetic lives, so the inline
// parser, the table model and the view cannot disagree about how wide a word is.
// Nothing here allocates or looks at a canvas; the functions are pure over the
// borrowed span.

// Display width of `length` bytes, decoding UTF-8 scalars.
size_t TerminalMarkdownText_cells(const char *text, size_t length);

// Bytes of `text` that fit in `cells` cells without splitting a scalar. A
// combining mark rides with the scalar before it. `usedCells` receives the width
// actually consumed (nullable).
size_t TerminalMarkdownText_bytesForCells(const char *text, size_t length, size_t cells,
                                  size_t *usedCells);

// Bytes of the first scalar, or 1 for malformed input.
size_t TerminalMarkdownText_scalarBytes(const char *text, size_t length);

// First byte offset of `needle` at or after `from`, or SIZE_MAX.
size_t TerminalMarkdownText_find(const char *text, size_t length, size_t from,
                         const char *needle, size_t needleLength);

// First byte offset of a run of exactly `run` copies of `glyph`, or SIZE_MAX.
size_t TerminalMarkdownText_findRun(const char *text, size_t length, size_t from, char glyph,
                            size_t run);

bool TerminalMarkdownText_isSpace(char character);
bool TerminalMarkdownText_isPunctuation(char character);

// Leading spaces/tabs, and the offset after the last non-space byte.
size_t TerminalMarkdownText_leadingSpaces(const char *text, size_t length);
size_t TerminalMarkdownText_trimRight(const char *text, size_t length);

// True when the span is empty or only spaces/tabs.
bool TerminalMarkdownText_blank(const char *text, size_t length);

#endif
