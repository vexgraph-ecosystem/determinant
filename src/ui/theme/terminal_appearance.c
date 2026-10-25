#include "ui/theme/terminal_appearance.h"
#include "format/terminal_color.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** Named appearance presets: a connector glyph set plus a colour palette, chosen
 * by name from an application's settings UI and applied by the app. The toolkit
 * stores no preference and reads no config — it only defines the presets and the
 * two apply calls. Glyphs are codepoints, so a preset may use real box drawing
 * ('─', '—', '═') now that cells are codepoints, not bytes. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalAppearance (ui/theme/terminal_appearance.c)
 * STRUCT FIELDS (Mirroring ui/theme/terminal_appearance.h)
 * TerminalRule rule            connector glyph set
 * uint32_t colorAccent, colorMuted, colorText, colorError   palette
 * FILE-LOCAL CONSTANTS
 * PRESETS[] / PRESET_NAMES[]   the named appearance table
 * FUNCTION REGISTRY (exports)
 * Default: TerminalAppearance_0(void)
 * Registry: presetCount, presetName(index), preset(index), named(name)
 * Config: set/getRule, set/getColors
 * Apply: TerminalAppearance_applyTo(self, frame),
 *   TerminalAppearance_drawRule(self, canvas, ly, pen)
 * Projections: TerminalProjection TerminalAppearance_toString/toStringStruct(self)
 * CONTRACT: an unknown name or out-of-range index yields the default; null self
 * is rejected; apply/draw no-op on null targets; no allocation, no logging.
 * ======================================================================== */

// Box-drawing and dash codepoints used by the presets.
#define CP_LIGHT_H  0x2500u   // ─
#define CP_LIGHT_V  0x2502u   // │
#define CP_LIGHT_T  0x252Cu   // ┬ (rail drops from the run)
#define CP_LIGHT_L  0x251Cu   // ├ (rail top, no run)
#define CP_EMDASH   0x2014u   // —
#define CP_DOUBLE_H 0x2550u   // ═
#define CP_DOUBLE_V 0x2551u   // ║
#define CP_DOUBLE_T 0x2566u   // ╦
#define CP_DOUBLE_L 0x2560u   // ╠
// Detached-box corners and cut endpoints. A panel gets its OWN corners instead of
// meeting its neighbours at a junction, and a divider stops short ('╶───╴').
#define CP_TL       0x250Cu   // ┌
#define CP_TR       0x2510u   // ┐
#define CP_BL       0x2514u   // └
#define CP_BR       0x2518u   // ┘
#define CP_CUT_L    0x2576u   // ╶
#define CP_CUT_R    0x2574u   // ╴
#define CP_DTL      0x2554u   // ╔
#define CP_DTR      0x2557u   // ╗
#define CP_DBL      0x255Au   // ╚
#define CP_DBR      0x255Du   // ╝
#define CP_CUT_DL   0x255Eu   // ╞
#define CP_CUT_DR   0x2561u   // ╡
// Every preset frames its panels with real corners and cuts its dividers short.
#define BOX_LIGHT   CP_TL, CP_TR, CP_BL, CP_BR, CP_CUT_L, CP_CUT_R
#define BOX_DOUBLE  CP_DTL, CP_DTR, CP_DBL, CP_DBR, CP_CUT_DL, CP_CUT_DR

typedef struct AppearancePreset {
    const char *name;
    TerminalRule rule;
} AppearancePreset;

static const AppearancePreset PRESETS[] = {
    { "ascii-solid",   { '-', '|', '+', '+', 0,            0, BOX_LIGHT } },
    { "ascii-striped", { '-', '|', '+', '+', '.',          4, BOX_LIGHT } },
    { "ascii-dashed",  { '-', '|', '+', '+', ' ',          2, BOX_LIGHT } },
    { "unicode-light", { CP_LIGHT_H, CP_LIGHT_V, CP_LIGHT_T, CP_LIGHT_L, 0, 0, BOX_LIGHT } },
    { "unicode-emdash",{ CP_EMDASH,  CP_LIGHT_V, CP_LIGHT_T, CP_LIGHT_L, 0, 0, BOX_LIGHT } },
    { "unicode-double",{ CP_DOUBLE_H, CP_DOUBLE_V, CP_DOUBLE_T, CP_DOUBLE_L, 0, 0, BOX_DOUBLE } },
};

static const size_t PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);
static const size_t PRESET_DEFAULT = 1;   // "ascii-striped"

static TerminalAppearance makeAppearance(size_t index) {
    TerminalAppearance appearance = {0};
    appearance.rule = PRESETS[index].rule;
    appearance.colorAccent = TERMINAL_RGB(0x7A, 0xC8, 0xFF);
    appearance.colorMuted = TERMINAL_RGB(0x88, 0x88, 0x88);
    appearance.colorText = TERMINAL_RGB(0xE6, 0xE6, 0xE6);
    appearance.colorError = TERMINAL_RGB(0xFF, 0x6B, 0x6B);
    return appearance;
}

TerminalAppearance TerminalAppearance_0(void) {
    return makeAppearance(PRESET_DEFAULT);
}

size_t TerminalAppearance_presetCount(void) { return PRESET_COUNT; }

const char *TerminalAppearance_presetName(size_t index) {
    return index < PRESET_COUNT ? PRESETS[index].name : nullptr;
}

TerminalAppearance TerminalAppearance_preset(size_t index) {
    return makeAppearance(index < PRESET_COUNT ? index : PRESET_DEFAULT);
}

TerminalAppearance TerminalAppearance_named(const char *name) {
    if (name != nullptr) {
        for (size_t i = 0; i < PRESET_COUNT; ++i)
            if (strcmp(name, PRESETS[i].name) == 0)
                return makeAppearance(i);
    }
    return makeAppearance(PRESET_DEFAULT);
}

bool TerminalAppearance_setRule(TerminalAppearance *self, TerminalRule rule) {
    if (self == nullptr) { THROW("TerminalAppearance rule: null self"); return false; }
    (*self).rule = rule;
    return true;
}
TerminalRule TerminalAppearance_getRule(const TerminalAppearance *self) {
    return self ? (*self).rule : TerminalRule_0();
}
bool TerminalAppearance_setColors(TerminalAppearance *self, uint32_t accent, uint32_t muted,
                                  uint32_t text, uint32_t error) {
    if (self == nullptr) { THROW("TerminalAppearance colors: null self"); return false; }
    (*self).colorAccent = accent;
    (*self).colorMuted = muted;
    (*self).colorText = text;
    (*self).colorError = error;
    return true;
}
TerminalAppearanceColors TerminalAppearance_getColors(const TerminalAppearance *self) {
    if (!self)
        return (TerminalAppearanceColors) { TERMINAL_COLOR_DEFAULT, TERMINAL_COLOR_DEFAULT, TERMINAL_COLOR_DEFAULT, TERMINAL_COLOR_DEFAULT };
    return (TerminalAppearanceColors) { (*self).colorAccent, (*self).colorMuted, (*self).colorText, (*self).colorError };
}

void TerminalAppearance_applyTo(const TerminalAppearance *self, TerminalFrame *frame) {
    if (self == nullptr || frame == nullptr)
        return;
    (void) TerminalFrame_setRule(frame, (*self).rule);
}

void TerminalAppearance_drawRule(const TerminalAppearance *self, TerminalCanvas *canvas,
                                 size_t ly, TerminalPen pen) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    TerminalRule_draw(&(*self).rule, canvas, ly, pen);
}

TerminalProjection TerminalAppearance_toString(const TerminalAppearance *self) {
    return TerminalAppearance_toStringStruct(self);
}

TerminalProjection TerminalAppearance_toStringStruct(const TerminalAppearance *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    TerminalRule rule = (*self).rule;
    return TerminalProjection_format(
        "TerminalAppearance{h=U+%04X,v=U+%04X,junction=U+%04X,stripe=U+%04X,every=%zu,accent=0x%08X}",
        rule.horizontal, rule.vertical, rule.junction,
        rule.stripe, rule.stripeEvery, (*self).colorAccent);
}
