#ifndef DETERMINANT_TERMINAL_BUTTON_H
#define DETERMINANT_TERMINAL_BUTTON_H
#include "ui/surface/terminal.h"
#include "ui/surface/terminal_projection.h"
typedef struct TerminalButton {
    const char *label;
    size_t x;
    size_t y;
    bool focused;
    bool enabled;
} TerminalButton;
typedef struct TerminalButtonPosition { size_t x; size_t y; } TerminalButtonPosition;
TerminalButton TerminalButton_0(void);
TerminalButton TerminalButton_3(const char *label, size_t x, size_t y);
#define TerminalButton(...) CONSTRUCTOR_DISPATCH(TerminalButton, __VA_ARGS__)
#define TerminalButton_zero() TerminalButton_0()
// Label borrowed immutable printable ASCII; setters reject without mutation.
bool TerminalButton_setLabel(TerminalButton *self, const char *label);
const char *TerminalButton_getLabel(const TerminalButton *self);
bool TerminalButton_setPosition(TerminalButton *self, size_t x, size_t y);
TerminalButtonPosition TerminalButton_getPosition(const TerminalButton *self);
void TerminalButton_setFocused(TerminalButton *self, bool focused);
bool TerminalButton_isFocused(const TerminalButton *self);
void TerminalButton_setEnabled(TerminalButton *self, bool enabled);
bool TerminalButton_isEnabled(const TerminalButton *self);
// Caller-fed normalized input; press returns activation, no retained callback.
// click acquires focus only for enabled button and returns true for an inside hit.
bool TerminalButton_key(TerminalButton *self, TerminalKey key);
bool TerminalButton_click(TerminalButton *self, size_t x, size_t y);
bool TerminalButton_render(const TerminalButton *self, Terminal *terminal);
TerminalProjection TerminalButton_toString(const TerminalButton *self);
TerminalProjection TerminalButton_toStringStruct(const TerminalButton *self);
#endif
