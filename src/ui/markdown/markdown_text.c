#include "ui/markdown/markdown_text.h"
#include "ui/surface/terminal_canvas.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <string.h>

;;DEFINITION
/** Cell measurement over borrowed Markdown spans. Every owner that places text
 * in a terminal row (paragraph, inline run, table cell, code line) measures the
 * same bytes the same way, so a wide scalar is two cells everywhere instead of
 * two cells in one path and one cell in another. Pure functions, no allocation,
 * no canvas, no locale. */
;;OVERVIEW
/** ============================================================================
 * CLASS: TerminalMarkdownText (ui/markdown/markdown_text.c)
 * STRUCT FIELDS: none — procedural measurement helpers.
 * FUNCTION REGISTRY (exports)
 * Width:    cells(text, length); bytesForCells(text, length, cells, used)
 * Scalar:   scalarBytes(text, length)
 * Search:   find(text, length, from, needle, needleLength)
 *           findRun(text, length, from, glyph, run)
 * Classify: isSpace(character); isPunctuation(character)
 * Trim:     leadingSpaces(text, length); trimRight(text, length); blank(...)
 * CONTRACT: malformed UTF-8 counts one cell and consumes one byte; a zero-width
 * scalar never starts a row by itself; a null span measures zero.
 * ============================================================================ */

size_t TerminalMarkdownText_cells(const char *text, size_t length) {
    if (text == nullptr)
        return 0;
    size_t cells = 0;
    size_t at = 0;
    while (at < length) {
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text + at, length - at);
        if (decoded.bytes == 0) {
            ++cells;
            ++at;
            continue;
        }
        cells += TerminalCanvas_widthOf(decoded.codepoint);
        at += decoded.bytes;
    }
    return cells;
}

size_t TerminalMarkdownText_bytesForCells(const char *text, size_t length, size_t cells,
                                  size_t *usedCells) {
    size_t at = 0;
    size_t used = 0;
    while (text != nullptr && at < length) {
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text + at, length - at);
        size_t width = decoded.bytes == 0 ? 1u : TerminalCanvas_widthOf(decoded.codepoint);
        if (width != 0 && used + width > cells)
            break;
        used += width;
        at += decoded.bytes == 0 ? 1u : decoded.bytes;
    }
    if (usedCells != nullptr)
        *usedCells = used;
    return at;
}

size_t TerminalMarkdownText_scalarBytes(const char *text, size_t length) {
    if (text == nullptr || length == 0)
        return 0;
    TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text, length);
    return decoded.bytes == 0 ? 1u : decoded.bytes;
}

size_t TerminalMarkdownText_find(const char *text, size_t length, size_t from,
                         const char *needle, size_t needleLength) {
    if (needle == nullptr || needleLength == 0 || from >= length
        || needleLength > length - from)
        return SIZE_MAX;
    for (size_t at = from; at + needleLength <= length; ++at)
        if (memcmp(text + at, needle, needleLength) == 0)
            return at;
    return SIZE_MAX;
}

size_t TerminalMarkdownText_findRun(const char *text, size_t length, size_t from, char glyph,
                            size_t run) {
    if (run == 0 || from >= length)
        return SIZE_MAX;
    size_t at = from;
    while (at < length) {
        if (text[at] != glyph) {
            ++at;
            continue;
        }
        size_t count = 0;
        while (at + count < length && text[at + count] == glyph)
            ++count;
        if (count == run)
            return at;
        at += count;
    }
    return SIZE_MAX;
}

bool TerminalMarkdownText_isSpace(char character) {
    return character == ' ' || character == '\t';
}

bool TerminalMarkdownText_isPunctuation(char character) {
    return (character >= '!' && character <= '/') || (character >= ':' && character <= '@')
        || (character >= '[' && character <= '`') || (character >= '{' && character <= '~');
}

size_t TerminalMarkdownText_leadingSpaces(const char *text, size_t length) {
    size_t at = 0;
    while (text != nullptr && at < length && TerminalMarkdownText_isSpace(text[at]))
        ++at;
    return at;
}

size_t TerminalMarkdownText_trimRight(const char *text, size_t length) {
    while (length > 0 && text != nullptr && TerminalMarkdownText_isSpace(text[length - 1u]))
        --length;
    return length;
}

bool TerminalMarkdownText_blank(const char *text, size_t length) {
    if (text == nullptr)
        return true;
    for (size_t at = 0; at < length; ++at)
        if (!TerminalMarkdownText_isSpace(text[at]))
            return false;
    return true;
}
