#ifndef DETERMINANT_TERMINAL_BUTTON_H
#define DETERMINANT_TERMINAL_BUTTON_H
#include "terminal/terminal.h"
typedef struct TerminalButton {
    const char *label;
    size_t x;
    size_t y;
    bool focused;
    bool enabled;
} TerminalButton;
TerminalButton TerminalButton_0(void);
TerminalButton TerminalButton_3(const char *label, size_t x, size_t y);
#define TerminalButton(...) CONSTRUCTOR_DISPATCH(TerminalButton, __VA_ARGS__)
#define TerminalButton_zero() TerminalButton_0()
// Label borrowed immutable printable ASCII; setters reject without mutation.
bool TerminalButton_setLabel(TerminalButton *self, const char *label);
const char *TerminalButton_getLabel(const TerminalButton *self);
bool TerminalButton_setPosition(TerminalButton *self, size_t x, size_t y);
void TerminalButton_getPosition(const TerminalButton *self, size_t *outX, size_t *outY);
void TerminalButton_setFocused(TerminalButton *self, bool focused);
bool TerminalButton_isFocused(const TerminalButton *self);
void TerminalButton_setEnabled(TerminalButton *self, bool enabled);
bool TerminalButton_isEnabled(const TerminalButton *self);
// Caller-fed normalized input; press returns activation, no retained callback.
// click acquires focus only for enabled button and returns true for an inside hit.
bool TerminalButton_key(TerminalButton *self, TerminalKey key);
bool TerminalButton_click(TerminalButton *self, size_t x, size_t y);
bool TerminalButton_render(const TerminalButton *self, Terminal *terminal);
bool TerminalButton_toString(const TerminalButton *self, char *dest, size_t cap, bool *outTruncated);
bool TerminalButton_toStringStruct(const TerminalButton *self, char *dest, size_t cap, bool *outTruncated);
#endif
