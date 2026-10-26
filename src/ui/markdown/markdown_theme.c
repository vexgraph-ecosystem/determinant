#include "ui/markdown/markdown_theme.h"
#include "format/terminal_color.h"
#include "format/terminal_style.h"
#include "annotation/definition.h"
#include "annotation/overview.h"

;;DEFINITION
/** The presentation ladder of a Markdown document: six heading pens, five callout
 * pens, and the body/code/link/quote/rule/table/copy pens around them. A theme is
 * a value record with no owned memory, resolved per span at render time, so a
 * heading level or a callout severity cannot collapse into a neighbour by
 * accident. The legacy eight-pen record is a projection of this value, not a
 * second source of truth. */
;;OVERVIEW
/** ============================================================================
 * CLASS: TerminalMarkdownTheme (ui/markdown/markdown_theme.c)
 * STRUCT FIELDS (Mirroring ui/markdown/markdown_theme.h)
 *   TerminalPen heading[6]   h1..h6, deliberately distinct
 *   TerminalPen body/marker/code/link/linkHover/quote/rule/fence
 *   TerminalPen callout[5]   NOTE, TIP, IMPORTANT, WARNING, CAUTION
 *   TerminalPen tableHeader/tableBorder/copy/copied
 *   const char *bar/copyGlyph/copiedGlyph   borrowed glyph strings
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalMarkdownTheme_0(void)
 * Resolve:      TerminalMarkdownTheme_pen(self, role, variant, hovered)
 * Projection:   fromStyle(style); TerminalMarkdownStyle style(self)
 * CONTRACT: null theme resolves to the default body pen; a heading variant
 * outside 1..6 clamps to h1/h6; an unknown callout kind falls back to body; the
 * returned pen never carries a span's emphasis flags — the caller ORs those.
 * ============================================================================ */

static uint8_t headingIndex(uint8_t level) {
    if (level == 0)
        return 0;
    return (uint8_t) (level - 1u > 5u ? 5u : level - 1u);
}

TerminalMarkdownTheme TerminalMarkdownTheme_0(void) {
    TerminalMarkdownTheme theme = {0};
    theme.heading[0] = TerminalPen_2(TERMINAL_STYLE_BOLD, TERMINAL_RGB(0xE8, 0xC4, 0x6A));
    theme.heading[1] = TerminalPen_2(TERMINAL_STYLE_BOLD, TERMINAL_RGB(0x8F, 0xC7, 0xE8));
    theme.heading[2] = TerminalPen_2(TERMINAL_STYLE_BOLD, TERMINAL_RGB(0xA8, 0xD8, 0xA0));
    theme.heading[3] = TerminalPen_2(TERMINAL_STYLE_BOLD | TERMINAL_STYLE_MUTED,
                                     TERMINAL_RGB(0xC9, 0xA6, 0xE0));
    theme.heading[4] = TerminalPen_2(TERMINAL_STYLE_MUTED, TERMINAL_RGB(0xD0, 0xD0, 0xD0));
    theme.heading[5] = TerminalPen_2(TERMINAL_STYLE_DIM, TERMINAL_RGB(0x9A, 0x9A, 0x9A));

    theme.body = TerminalPen_0();
    theme.marker = TerminalPen_2(TERMINAL_STYLE_BOLD, TERMINAL_RGB(0xEB, 0xC0, 0x5F));
    theme.code = TerminalPen_2(TERMINAL_STYLE_NONE, TERMINAL_RGB(0x70, 0xBE, 0x82));
    theme.link = TerminalPen_2(TERMINAL_STYLE_UNDERLINE, TERMINAL_RGB(0x6F, 0xC3, 0xE8));
    theme.linkHover = TerminalPen_2(TERMINAL_STYLE_BOLD | TERMINAL_STYLE_UNDERLINE,
                                    TERMINAL_RGB(0xFF, 0xD4, 0x79));
    theme.quote = TerminalPen_2(TERMINAL_STYLE_BOLD, TERMINAL_RGB(0x69, 0xBE, 0xDC));
    theme.rule = TerminalPen_1(TERMINAL_STYLE_DIM);
    // The wash is a neutral grey at 40% alpha, composited by the device backend
    // over its own backdrop, so one value reads on a dark and a light terminal.
    theme.fence = TerminalPen_3(TERMINAL_STYLE_DIM, TERMINAL_COLOR_DEFAULT,
                                TERMINAL_RGBA(0xA0, 0xA0, 0xA0, 0x44));

    theme.callout[0] = TerminalPen_2(TERMINAL_STYLE_NONE, TERMINAL_RGB(0x6F, 0xA8, 0xDC));
    theme.callout[1] = TerminalPen_2(TERMINAL_STYLE_NONE, TERMINAL_RGB(0x7F, 0xCF, 0x8F));
    theme.callout[2] = TerminalPen_2(TERMINAL_STYLE_NONE, TERMINAL_RGB(0xC9, 0xA6, 0xE0));
    theme.callout[3] = TerminalPen_2(TERMINAL_STYLE_NONE, TERMINAL_RGB(0xEB, 0xC0, 0x5F));
    theme.callout[4] = TerminalPen_2(TERMINAL_STYLE_NONE, TERMINAL_RGB(0xE0, 0x70, 0x70));

    theme.tableHeader = TerminalPen_2(TERMINAL_STYLE_BOLD | TERMINAL_STYLE_ACCENT,
                                      TERMINAL_RGB(0xD8, 0xC4, 0x8A));
    theme.tableBorder = TerminalPen_2(TERMINAL_STYLE_DIM, TERMINAL_RGB(0x80, 0x80, 0x80));
    theme.copy = TerminalPen_2(TERMINAL_STYLE_MUTED, TERMINAL_RGB(0x9A, 0xA0, 0xA6));
    theme.copied = TerminalPen_2(TERMINAL_STYLE_BOLD, TERMINAL_RGB(0x7F, 0xCF, 0x8F));

    theme.bar = TerminalMarkdownCallout_bar();
    theme.copyGlyph = "⧉";
    theme.copiedGlyph = "✓";
    return theme;
}

TerminalPen TerminalMarkdownTheme_pen(const TerminalMarkdownTheme *self,
                                      TerminalMarkdownRole role, uint8_t variant,
                                      bool hovered) {
    if (self == nullptr)
        return TerminalPen_0();
    switch (role) {
        case TERMINAL_MARKDOWN_HEADING:
            return (*self).heading[headingIndex(variant)];
        case TERMINAL_MARKDOWN_CODE:
            return (*self).code;
        case TERMINAL_MARKDOWN_LINK:
            return hovered ? (*self).linkHover : (*self).link;
        case TERMINAL_MARKDOWN_QUOTE:
            return (*self).quote;
        case TERMINAL_MARKDOWN_RULE:
            return (*self).rule;
        case TERMINAL_MARKDOWN_MARKER:
            return (*self).marker;
        case TERMINAL_MARKDOWN_FENCE:
            return (*self).fence;
        case TERMINAL_MARKDOWN_CALLOUT: {
            size_t index = TerminalMarkdownCallout_penIndex((TerminalMarkdownCalloutKind) variant);
            return index < TERMINAL_MARKDOWN_CALLOUT_KIND_COUNT ? (*self).callout[index] : (*self).body;
        }
        case TERMINAL_MARKDOWN_TABLE_HEADER:
            return (*self).tableHeader;
        case TERMINAL_MARKDOWN_TABLE_BORDER:
            return (*self).tableBorder;
        default:
            return (*self).body;
    }
}

TerminalMarkdownTheme TerminalMarkdownTheme_fromStyle(TerminalMarkdownStyle style) {
    TerminalMarkdownTheme theme = TerminalMarkdownTheme_0();
    for (size_t level = 0; level < 6u; ++level)
        theme.heading[level] = style.heading;
    theme.body = style.body;
    theme.code = style.code;
    theme.link = style.link;
    theme.quote = style.quote;
    theme.rule = style.rule;
    theme.marker = style.marker;
    theme.fence = style.fence;
    return theme;
}

TerminalMarkdownStyle TerminalMarkdownTheme_style(const TerminalMarkdownTheme *self) {
    if (self == nullptr)
        return (TerminalMarkdownStyle) {0};
    return (TerminalMarkdownStyle) {
        (*self).body, (*self).heading[0], (*self).code, (*self).link,
        (*self).quote, (*self).rule, (*self).marker, (*self).fence
    };
}
