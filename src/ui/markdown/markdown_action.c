#include "ui/markdown/markdown_action.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <stdint.h>

;;DEFINITION
/** The value a preview click resolves to. A link carries a borrowed target; a
 * code block carries its index in the fence pool. The view never performs the
 * action, so it never owns a clipboard, a clock or a handler — it only reports
 * which affordance a cell belongs to. */
;;OVERVIEW
/** ============================================================================
 * CLASS: TerminalMarkdownAction (ui/markdown/markdown_action.c)
 * STRUCT FIELDS: none — the value record is declared in the header.
 * FUNCTION REGISTRY (exports)
 * Geometry: iconColumn(width, glyphCells); iconHit(width, column, glyphCells)
 * String:   toString/toStringStruct
 * CONTRACT: the icon keeps one cell of right margin; a row too narrow for the
 * glyph reports column 0 and never a negative/underflowed position; a NONE
 * action has no target and no block.
 * ============================================================================ */

size_t TerminalMarkdownAction_iconColumn(size_t width, size_t glyphCells) {
    if (glyphCells == 0 || width < glyphCells + 1u)
        return 0;
    return width - glyphCells - 1u;
}

bool TerminalMarkdownAction_iconHit(size_t width, size_t column, size_t glyphCells) {
    if (glyphCells == 0 || width < glyphCells + 1u)
        return false;
    size_t start = TerminalMarkdownAction_iconColumn(width, glyphCells);
    return column >= start && column < start + glyphCells;
}

TerminalProjection TerminalMarkdownAction_toString(TerminalMarkdownAction action) {
    switch (action.kind) {
        case TERMINAL_MARKDOWN_ACTION_LINK:
            return TerminalProjection_format("link{target=%.*s}",
                                             (int) action.targetLength,
                                             action.target != nullptr ? action.target : "");
        case TERMINAL_MARKDOWN_ACTION_COPY_CODE:
            return TerminalProjection_format("copy{block=%zu}", action.block);
        default:
            return TerminalProjection_format("none");
    }
}

TerminalProjection TerminalMarkdownAction_toStringStruct(TerminalMarkdownAction action) {
    return TerminalProjection_format(
        "TerminalMarkdownAction{kind=%d,target=%s,targetLength=%zu,block=%zu}",
        (int) action.kind, action.target != nullptr ? "<borrowed>" : "nullptr",
        action.targetLength, action.block);
}
