#ifndef DETERMINANT_TERMINAL_APPEARANCE_H
#define DETERMINANT_TERMINAL_APPEARANCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "c23/constructor.h"

#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"
#include "ui/panel/terminal_frame.h"

// terminal/terminal_appearance.h — the toolkit's NAMED APPEARANCE presets.
//
// A look is a named, self-contained record: the connector glyph set (ASCII or
// real box drawing) plus the colour palette. Applications list the presets, let
// the user pick one in their own settings UI, persist the NAME, and apply it —
// the toolkit never stores preferences and never reads a config file.
//
// Presets (codepoint glyphs, so a preset may use '─' / '—' / '═' as well as
// '-'):
//   "ascii-solid"    `----------------------------`
//   "ascii-striped"  `---.---.---.---.---.---.---`   (default)
//   "ascii-dashed"   `- - - - - - - - - - - - - -`
//   "unicode-light"  `──────────┬───────────┬─────`
//   "unicode-emdash" `——————————┬———————————┬—————`
//   "unicode-double" `══════════╦═══════════╦═════`
// Every preset also carries accent/muted/text/error colours.

typedef struct TerminalAppearance {
    TerminalRule rule;
    uint32_t colorAccent;
    uint32_t colorMuted;
    uint32_t colorText;
    uint32_t colorError;
} TerminalAppearance;
typedef struct TerminalAppearanceColors { uint32_t accent; uint32_t muted; uint32_t text; uint32_t error; } TerminalAppearanceColors;

// The default appearance (the "ascii-striped" preset).
TerminalAppearance TerminalAppearance_0(void);
#define TerminalAppearance(...) CONSTRUCTOR_DISPATCH(TerminalAppearance, __VA_ARGS__)

// --- Preset registry (for a settings UI: list names, pick by name/index) ---
size_t      TerminalAppearance_presetCount(void);
const char *TerminalAppearance_presetName(size_t index);      // NULL when out of range
TerminalAppearance TerminalAppearance_preset(size_t index);   // default when out of range
TerminalAppearance TerminalAppearance_named(const char *name);// default when unknown

// --- Configuration (symmetric setters & getters) ---
bool TerminalAppearance_setRule(TerminalAppearance *self, TerminalRule rule);
TerminalRule TerminalAppearance_getRule(const TerminalAppearance *self);
bool TerminalAppearance_setColors(TerminalAppearance *self, uint32_t accent, uint32_t muted,
                                  uint32_t text, uint32_t error);
TerminalAppearanceColors TerminalAppearance_getColors(const TerminalAppearance *self);

// --- Apply ---
// Push the preset's rule into a frame; draw a rule run into any canvas region.
void TerminalAppearance_applyTo(const TerminalAppearance *self, TerminalFrame *frame);
void TerminalAppearance_drawRule(const TerminalAppearance *self, TerminalCanvas *canvas,
                                 size_t ly, TerminalPen pen);

TerminalProjection TerminalAppearance_toString(const TerminalAppearance *self);
TerminalProjection TerminalAppearance_toStringStruct(const TerminalAppearance *self);

#endif
