#include "terminal/terminal_button.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** A reusable terminal-cell button borrows its immutable label and paints into
 * a resident Terminal grid. Host-normalized keys and click coordinates feed
 * interaction, never stdin. Activation is a returned event, not owned code;
 * consumer consent/tool/application policy remains outside this widget. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalButton (terminal/terminal_button.c)
 * STRUCT FIELDS (Mirroring terminal/terminal_button.h)
 * const char *label         borrowed printable ASCII label
 * size_t x                 left column
 * size_t y                 row
 * bool focused             admitted keyboard focus
 * bool enabled             interaction enabled
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalButton TerminalButton_0(void)
 *   TerminalButton TerminalButton_3(const char *label, size_t x, size_t y)
 * Setters: bool TerminalButton_setLabel(TerminalButton *self, const char *label)
 *   bool TerminalButton_setPosition(TerminalButton *self, size_t x, size_t y)
 *   void TerminalButton_setFocused(TerminalButton *self, bool focused)
 *   void TerminalButton_setEnabled(TerminalButton *self, bool enabled)
 * Getters: const char *TerminalButton_getLabel(const TerminalButton *self)
 *   void TerminalButton_getPosition(const TerminalButton *self,
 *     size_t *outX, size_t *outY)
 *   bool TerminalButton_isFocused(const TerminalButton *self)
 *   bool TerminalButton_isEnabled(const TerminalButton *self)
 * Input: bool TerminalButton_key(TerminalButton *self, TerminalKey key)
 *   bool TerminalButton_click(TerminalButton *self, size_t x, size_t y)
 * Painting: bool TerminalButton_render(const TerminalButton *self, Terminal *terminal)
 * Projections: TerminalProjection TerminalButton_toString/toStringStruct(self)
 * PRIVATE HELPERS: validLabel(label) checks printable ASCII/size arithmetic.
 * CONTRACT: externally serialized; borrowed label must remain immutable/live.
 * Default empty label at 0,0, unfocused/enabled. Disabling clears focus; setting
 * focus on disabled has no effect. Enabled inside click focuses and activates;
 * outside click loses focus. Enter/Space activate once per fed key only when
 * focused/enabled; Escape loses focus. No repeats/timing/device decoding owned.
 * Paint clips by rejection before writing, preserving grid on failure; brackets
 * denote normal/focused/disabled. Projections redact label (no secret copying).
 * ======================================================================== */
static bool validLabel(const char *label) {
    if (label == nullptr)
        return false;
    size_t n = strlen(label);
    if (n > SIZE_MAX - 2)
        return false;
    for (size_t i = 0; i < n; ++i)
        if ((unsigned char) label[i] < 32 || (unsigned char) label[i] >= 127)
            return false;
    return true;
}
TerminalButton TerminalButton_0(void) { return (TerminalButton) {.label = "", .enabled = true}; }
TerminalButton TerminalButton_3(const char *label, size_t x, size_t y) {
    TerminalButton self = TerminalButton_0();
    if (!TerminalButton_setLabel(&self, label) || !TerminalButton_setPosition(&self, x, y))
        return TerminalButton_0();
    return self;
}
bool TerminalButton_setLabel(TerminalButton *self, const char *label) {
    if (self == nullptr || !validLabel(label) || strlen(label) + 2 > SIZE_MAX - (*self).x) {
        THROW("TerminalButton setLabel: invalid label/extent");
        return false;
    }
    (*self).label = label;
    return true;
}
const char *TerminalButton_getLabel(const TerminalButton *self) { return self ? (*self).label : nullptr; }
bool TerminalButton_setPosition(TerminalButton *self, size_t x, size_t y) {
    if (self == nullptr || strlen((*self).label) + 2 > SIZE_MAX - x) {
        THROW("TerminalButton setPosition: invalid extent");
        return false;
    }
    (*self).x = x;
    (*self).y = y;
    return true;
}
TerminalButtonPosition TerminalButton_getPosition(const TerminalButton *self) {
    return self ? (TerminalButtonPosition) { (*self).x, (*self).y } : (TerminalButtonPosition) { 0u, 0u };
}
void TerminalButton_setFocused(TerminalButton *self, bool focused) {
    if (self == nullptr) {
        THROW("TerminalButton setFocused: null button");
        return;
    }
    (*self).focused = focused && (*self).enabled;
}
bool TerminalButton_isFocused(const TerminalButton *self) { return self && (*self).focused; }
void TerminalButton_setEnabled(TerminalButton *self, bool enabled) {
    if (self == nullptr) {
        THROW("TerminalButton setEnabled: null button");
        return;
    }
    (*self).enabled = enabled;
    if (!enabled)
        (*self).focused = false;
}
bool TerminalButton_isEnabled(const TerminalButton *self) { return self && (*self).enabled; }
bool TerminalButton_key(TerminalButton *self, TerminalKey key) {
    if (self == nullptr || key < TERMINAL_KEY_NONE || key > TERMINAL_KEY_DOWN) {
        THROW("TerminalButton key: invalid input");
        return false;
    }
    if (key == TERMINAL_KEY_ESCAPE)
        (*self).focused = false;
    return (*self).enabled && (*self).focused && (key == TERMINAL_KEY_ENTER || key == TERMINAL_KEY_SPACE);
}
bool TerminalButton_click(TerminalButton *self, size_t x, size_t y) {
    if (self == nullptr) {
        THROW("TerminalButton click: null button");
        return false;
    }
    bool hit = (*self).enabled && y == (*self).y && x >= (*self).x
        && x - (*self).x < strlen((*self).label) + 2;
    (*self).focused = hit;
    return hit;
}
bool TerminalButton_render(const TerminalButton *self, Terminal *terminal) {
    if (self == nullptr || terminal == nullptr) {
        THROW("TerminalButton render: null input");
        return false;
    }
    size_t length = strlen((*self).label);
    size_t width = Terminal_getWidth(terminal);
    if ((*self).y >= Terminal_getHeight(terminal) || (*self).x > width || length + 2 > width - (*self).x) {
        THROW("TerminalButton render: outside cell grid");
        return false;
    }
    const char *left = !(*self).enabled ? "(" : ((*self).focused ? "{" : "[");
    const char *right = !(*self).enabled ? ")" : ((*self).focused ? "}" : "]");
    return Terminal_text(terminal, (*self).x, (*self).y, left, 1)
        && Terminal_text(terminal, (*self).x + 1, (*self).y, (*self).label, length)
        && Terminal_text(terminal, (*self).x + 1 + length, (*self).y, right, 1);
}
TerminalProjection TerminalButton_toString(const TerminalButton *self) {
    return TerminalButton_toStringStruct(self);
}
TerminalProjection TerminalButton_toStringStruct(const TerminalButton *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalButton{label=<borrowed>,x=%zu,y=%zu,focused=%d,enabled=%d}",
        (*self).x, (*self).y, (*self).focused, (*self).enabled);
}
