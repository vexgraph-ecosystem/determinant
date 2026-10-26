#include "ui/markdown/markdown_callout.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <strings.h>
#include <string.h>

;;DEFINITION
/** The severity vocabulary of a quoted aside, and the two source spellings that
 * select it. GFM's `[!WARNING]` and the hand-written `!WARNING!` are one kind;
 * an unrecognised name is not a callout at all, so malformed markup degrades to
 * the literal text the author typed instead of inventing a severity. */
;;OVERVIEW
/** ============================================================================
 * CLASS: TerminalMarkdownCallout (ui/markdown/markdown_callout.c)
 * STRUCT FIELDS: none — value records are declared in the header.
 * FUNCTION REGISTRY (exports)
 * Parse:  TerminalMarkdownCallout_parse(text, length)
 * Names:  TerminalMarkdownCallout_name(kind); TerminalMarkdownCallout_bar(void)
 * Theme:  TerminalMarkdownCallout_penIndex(kind)
 * CONTRACT: the marker must start the span; `[!KIND]` needs its closing bracket
 * and `!KIND!` its closing bang; the title is everything after one optional
 * space; kind matching is ASCII case-insensitive; no allocation.
 * ============================================================================ */

typedef struct CalloutEntry {
    const char *name;
    TerminalMarkdownCalloutKind kind;
} CalloutEntry;

static const CalloutEntry CALLOUT_ENTRIES[] = {
    { "NOTE", TERMINAL_MARKDOWN_CALLOUT_NOTE },
    { "TIP", TERMINAL_MARKDOWN_CALLOUT_TIP },
    { "IMPORTANT", TERMINAL_MARKDOWN_CALLOUT_IMPORTANT },
    { "WARNING", TERMINAL_MARKDOWN_CALLOUT_WARNING },
    { "CAUTION", TERMINAL_MARKDOWN_CALLOUT_CAUTION },
};

static TerminalMarkdownCalloutKind kindForName(const char *name, size_t length) {
    for (size_t index = 0; index < sizeof(CALLOUT_ENTRIES) / sizeof(CALLOUT_ENTRIES[0]); ++index) {
        const char *candidate = CALLOUT_ENTRIES[index].name;
        if (strlen(candidate) == length && strncasecmp(candidate, name, length) == 0)
            return CALLOUT_ENTRIES[index].kind;
    }
    return TERMINAL_MARKDOWN_CALLOUT_NONE;
}

TerminalMarkdownCallout TerminalMarkdownCallout_parse(const char *text, size_t length) {
    TerminalMarkdownCallout callout = {0};
    if (text == nullptr || length < 4u)
        return callout;
    size_t nameStart = 0;
    size_t nameEnd = 0;
    size_t markerEnd = 0;
    if (text[0] == '[' && text[1] == '!') {
        size_t close = 2u;
        while (close < length && text[close] != ']')
            ++close;
        if (close >= length)
            return callout;
        nameStart = 2u;
        nameEnd = close;
        markerEnd = close + 1u;
    } else if (text[0] == '!') {
        size_t close = 1u;
        while (close < length && text[close] != '!')
            ++close;
        if (close >= length)
            return callout;
        nameStart = 1u;
        nameEnd = close;
        markerEnd = close + 1u;
    } else {
        return callout;
    }
    if (nameEnd <= nameStart)
        return callout;
    TerminalMarkdownCalloutKind kind = kindForName(text + nameStart, nameEnd - nameStart);
    if (kind == TERMINAL_MARKDOWN_CALLOUT_NONE)
        return callout;
    size_t content = markerEnd;
    if (content < length && text[content] == ' ')
        ++content;
    callout.valid = true;
    callout.kind = kind;
    callout.contentStart = content;
    callout.titleStart = content;
    callout.titleLength = length - content;
    return callout;
}

const char *TerminalMarkdownCallout_name(TerminalMarkdownCalloutKind kind) {
    for (size_t index = 0; index < sizeof(CALLOUT_ENTRIES) / sizeof(CALLOUT_ENTRIES[0]); ++index)
        if (CALLOUT_ENTRIES[index].kind == kind)
            return CALLOUT_ENTRIES[index].name;
    return "";
}

const char *TerminalMarkdownCallout_bar(void) {
    return TERMINAL_MARKDOWN_CALLOUT_BAR;
}

size_t TerminalMarkdownCallout_penIndex(TerminalMarkdownCalloutKind kind) {
    if (kind <= TERMINAL_MARKDOWN_CALLOUT_NONE || kind >= TERMINAL_MARKDOWN_CALLOUT_KIND_COUNT)
        return TERMINAL_MARKDOWN_CALLOUT_KIND_COUNT;
    return (size_t) kind - 1u;
}
