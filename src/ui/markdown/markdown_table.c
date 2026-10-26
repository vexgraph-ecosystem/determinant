#include "ui/markdown/markdown_table.h"
#include "ui/markdown/markdown_text.h"
#include "ui/surface/terminal_canvas.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <stdint.h>

;;DEFINITION
/** The pipe-table model: alignment from the delimiter row, escape- and
 * code-aware cell splitting, and a width policy that prefers each column's
 * natural width, falls back to its longest word, and only then truncates inside
 * a word — and when it truncates it says so, so the caller can draw an ellipsis
 * instead of pretending the cell ended there. No terminal, no pen, no owned
 * memory. */
;;OVERVIEW
/** ============================================================================
 * CLASS: TerminalMarkdownTable (ui/markdown/markdown_table.c)
 * STRUCT FIELDS: none — the cell-line value record is declared in the header.
 * FUNCTION REGISTRY (exports)
 * Delimiter: parseDelimiter(text, length, aligns, capacity)
 * Row:       split(text, length, cells, lengths, capacity)
 * Measure:   natural(text, length); longestWord(text, length)
 *            lineCount(text, length, width)
 * Wrap:      cellLine(text, length, offset, width)
 * Allocate:  allocate(needed, minimums, widths, columns, viewport)
 * CONTRACT: `\|` and pipes inside a backtick run never split a cell; an optional
 * outer pipe is ignored; an empty interior cell is a real column; truncation
 * leaves one cell free for the caller's ellipsis.
 * ============================================================================ */

static size_t cellTrimStart(const char *text, size_t start, size_t end) {
    while (start < end && TerminalMarkdownText_isSpace(text[start]))
        ++start;
    return start;
}

static size_t cellTrimEnd(const char *text, size_t start, size_t end) {
    while (end > start && TerminalMarkdownText_isSpace(text[end - 1u]))
        --end;
    return end;
}

static size_t skipCodeRun(const char *text, size_t at, size_t end) {
    size_t run = 0;
    while (at + run < end && text[at + run] == '`')
        ++run;
    size_t close = TerminalMarkdownText_findRun(text, end, at + run, '`', run);
    return close == SIZE_MAX ? at + run : close + run;
}

size_t TerminalMarkdownTable_split(const char *text, size_t length, const char **cells,
                           size_t *lengths, size_t capacity) {
    if (text == nullptr || cells == nullptr || lengths == nullptr || capacity == 0)
        return 0;
    size_t start = TerminalMarkdownText_leadingSpaces(text, length);
    size_t end = TerminalMarkdownText_trimRight(text, length);
    if (start < end && text[start] == '|')
        ++start;
    if (end > start && text[end - 1u] == '|')
        --end;
    size_t count = 0;
    size_t cellStart = start;
    size_t at = start;
    while (at <= end) {
        bool boundary = false;
        if (at == end) {
            boundary = true;
        } else if (text[at] == '\\' && at + 1u < end) {
            at += 2u;
            continue;
        } else if (text[at] == '`') {
            size_t next = skipCodeRun(text, at, end);
            at = next > at ? next : at + 1u;
            continue;
        } else if (text[at] == '|') {
            boundary = true;
        }
        if (!boundary) {
            ++at;
            continue;
        }
        size_t trimmedStart = cellTrimStart(text, cellStart, at);
        size_t trimmedEnd = cellTrimEnd(text, trimmedStart, at);
        if (count < capacity) {
            cells[count] = text + trimmedStart;
            lengths[count] = trimmedEnd - trimmedStart;
        }
        ++count;
        if (at == end)
            break;
        cellStart = at + 1u;
        at = cellStart;
    }
    return count;
}

size_t TerminalMarkdownTable_parseDelimiter(const char *text, size_t length,
                                    TerminalMarkdownTableAlign *aligns, size_t capacity) {
    const char *cells[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    size_t lengths[TERMINAL_MARKDOWN_TABLE_COLUMNS];
    size_t count = TerminalMarkdownTable_split(text, length, cells, lengths, TERMINAL_MARKDOWN_TABLE_COLUMNS);
    if (count < 2u)
        return 0;
    for (size_t column = 0; column < count; ++column) {
        bool leftColon = false;
        bool rightColon = false;
        size_t dashes = 0;
        for (size_t at = 0; at < lengths[column]; ++at) {
            char character = cells[column][at];
            if (character == ':') {
                if (at == 0)
                    leftColon = true;
                else if (at + 1u == lengths[column])
                    rightColon = true;
                else
                    return 0;
            } else if (character == '-') {
                ++dashes;
            } else {
                return 0;
            }
        }
        if (dashes == 0)
            return 0;
        if (aligns != nullptr && column < capacity)
            aligns[column] = leftColon && rightColon ? TERMINAL_MARKDOWN_TABLE_CENTER
                           : rightColon ? TERMINAL_MARKDOWN_TABLE_RIGHT : TERMINAL_MARKDOWN_TABLE_LEFT;
    }
    return count;
}

size_t TerminalMarkdownTable_natural(const char *text, size_t length) {
    return TerminalMarkdownText_cells(text, length);
}

size_t TerminalMarkdownTable_longestWord(const char *text, size_t length) {
    size_t longest = 0;
    size_t word = 0;
    size_t at = 0;
    while (text != nullptr && at < length) {
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text + at, length - at);
        size_t bytes = decoded.bytes == 0 ? 1u : decoded.bytes;
        if (decoded.bytes == 0 || TerminalMarkdownText_isSpace(text[at])) {
            if (word > longest)
                longest = word;
            word = 0;
        } else {
            word += TerminalCanvas_widthOf(decoded.codepoint);
        }
        at += bytes;
    }
    if (word > longest)
        longest = word;
    return longest == 0 ? 1u : longest;
}

TerminalMarkdownTableCellLine TerminalMarkdownTable_cellLine(const char *text, size_t length,
                                             size_t offset, size_t width) {
    TerminalMarkdownTableCellLine line = { text + length, 0, length, 0, false };
    if (text == nullptr)
        return line;
    if (offset > length)
        offset = length;
    while (offset < length && TerminalMarkdownText_isSpace(text[offset]))
        ++offset;
    line.text = text + offset;
    line.next = offset;
    if (offset == length)
        return line;
    if (width == 0) {
        line.truncated = true;
        return line;
    }
    size_t at = offset;
    size_t used = 0;
    size_t lastSpace = SIZE_MAX;
    size_t afterSpace = SIZE_MAX;
    while (at < length) {
        if (TerminalMarkdownText_isSpace(text[at]) && at > offset) {
            lastSpace = at;
            afterSpace = at + 1u;
            while (afterSpace < length && TerminalMarkdownText_isSpace(text[afterSpace]))
                ++afterSpace;
        }
        TerminalCanvasDecodedScalar decoded = TerminalCanvas_utf8Decode(text + at, length - at);
        size_t bytes = decoded.bytes == 0 ? 1u : decoded.bytes;
        size_t glyphCells = decoded.bytes == 0 ? 1u : TerminalCanvas_widthOf(decoded.codepoint);
        if (glyphCells != 0 && used + glyphCells > width) {
            if (lastSpace != SIZE_MAX) {
                line.length = lastSpace - offset;
                line.next = afterSpace;
                line.cells = TerminalMarkdownText_cells(line.text, line.length);
                return line;
            }
            size_t budget = width > 1u ? width - 1u : 0u;
            size_t take = TerminalMarkdownText_bytesForCells(text + offset, length - offset,
                                                     budget, &used);
            if (take == 0) {
                take = TerminalMarkdownText_scalarBytes(text + offset, length - offset);
                used = TerminalMarkdownText_cells(text + offset, take);
            }
            line.length = take;
            line.next = offset + take;
            line.cells = used;
            line.truncated = true;
            return line;
        }
        used += glyphCells;
        at += bytes;
    }
    line.length = at - offset;
    line.next = length;
    line.cells = used;
    return line;
}

size_t TerminalMarkdownTable_lineCount(const char *text, size_t length, size_t width) {
    size_t offset = 0;
    size_t count = 0;
    while (offset < length) {
        TerminalMarkdownTableCellLine line = TerminalMarkdownTable_cellLine(text, length, offset, width);
        ++count;
        if (line.next <= offset)
            break;
        offset = line.next;
        if (TerminalMarkdownText_blank(text + (offset < length ? offset : length),
                               length - (offset < length ? offset : length)))
            break;
    }
    return count == 0 ? 1u : count;
}

bool TerminalMarkdownTable_allocate(const size_t *needed, const size_t *minimums, size_t *widths,
                            size_t columns, size_t viewport) {
    if (needed == nullptr || minimums == nullptr || widths == nullptr || columns == 0)
        return false;
    size_t overhead = columns * 3u + 1u;
    if (viewport <= overhead)
        return false;
    size_t available = viewport - overhead;
    size_t neededTotal = 0;
    size_t minimumTotal = 0;
    bool neededFits = true;
    bool minimumsFit = true;
    for (size_t column = 0; column < columns; ++column) {
        if (needed[column] > available - neededTotal) {
            neededFits = false;
            neededTotal = available;
        } else if (neededFits) {
            neededTotal += needed[column];
        }
        if (minimums[column] > available - minimumTotal) {
            minimumsFit = false;
            minimumTotal = available;
        } else if (minimumsFit) {
            minimumTotal += minimums[column];
        }
        widths[column] = minimums[column];
    }
    if (!minimumsFit)
        return false;
    if (neededFits) {
        for (size_t column = 0; column < columns; ++column)
            widths[column] = needed[column];
        return true;
    }
    size_t extraBudget = available - minimumTotal;
    size_t remainingNeed = 0;
    for (size_t column = 0; column < columns; ++column) {
        size_t extra = needed[column] > minimums[column] ? needed[column] - minimums[column] : 0u;
        remainingNeed = extra > SIZE_MAX - remainingNeed ? SIZE_MAX : remainingNeed + extra;
    }
    if (remainingNeed == 0u)
        return true;
    size_t used = 0;
    long double fractions[TERMINAL_MARKDOWN_TABLE_COLUMNS] = {0};
    for (size_t column = 0; column < columns; ++column) {
        size_t extra = needed[column] > minimums[column] ? needed[column] - minimums[column] : 0u;
        long double share = (long double) extraBudget * (long double) extra
                          / (long double) remainingNeed;
        size_t whole = (size_t) share;
        if (whole > extra)
            whole = extra;
        widths[column] += whole;
        used += whole;
        fractions[column] = whole == extra ? -1.0L : share - (long double) whole;
    }
    while (used < extraBudget) {
        size_t chosen = SIZE_MAX;
        for (size_t column = 0; column < columns; ++column)
            if (fractions[column] >= 0.0L
                && (chosen == SIZE_MAX || fractions[column] > fractions[chosen]))
                chosen = column;
        if (chosen == SIZE_MAX)
            break;
        ++widths[chosen];
        ++used;
        fractions[chosen] = -1.0L;
    }
    return true;
}
