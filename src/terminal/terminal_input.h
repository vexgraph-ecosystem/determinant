#ifndef DETERMINANT_TERMINAL_INPUT_H
#define DETERMINANT_TERMINAL_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "terminal/terminal_projection.h"
#include "c23/constructor.h"

// terminal/terminal_input.h — the terminal's MOUSE byte decoder.
//
// A terminal reports the mouse as escape sequences on the SAME byte stream as
// keys. TerminalInput turns those bytes into one normalized event in CELLS, so
// nothing above it has to know an escape grammar and nothing below it has to know
// a cell. Bytes in, event out: no I/O, no allocation, no clock, which is also why
// the whole thing is provable with literal byte strings and no terminal.
//
// Two encodings arrive in practice:
//   SGR (1006, the modern one)   ESC [ < b ; x ; y M   press / motion
//                                ESC [ < b ; x ; y m   release
//   X10 (legacy fallback)        ESC [ M then exactly three bytes: b+32, x+32, y+32
//
// The button field `b` packs everything:
//   bits 0-1  button: 0 left, 1 middle, 2 right, 3 none
//   bit 2     shift, bit 3 meta, bit 4 ctrl
//   bit 5     motion (SGR only — this is what makes hover possible at all)
//   bit 6     wheel: 64 up, 65 down, 66 left, 67 right
//
// X10 cannot express motion, modifiers beyond its three raw bytes, or a
// coordinate past 223, which is exactly why SGR exists.
//
// Coordinates arrive 1-based and leave 0-based, so an event can index a canvas
// cell directly. Feeding is a state machine rather than a buffer parser because
// the host receives the stream one byte at a time; a malformed sequence is
// counted and abandoned, never reported per byte (this is a hot path).
//
// The host owns the terminal mode that decides which events it can receive:
// enabling 1003 + 1006 is what delivers motion to this decoder, and disabling
// reporting is what hands the mouse back to the terminal's own selection.

typedef enum TerminalPointerKind {
    TERMINAL_POINTER_NONE = 0,
    TERMINAL_POINTER_MOVE,       // motion with no button held — a hover
    TERMINAL_POINTER_DRAG,       // motion with a button held
    TERMINAL_POINTER_PRESS,
    TERMINAL_POINTER_RELEASE,
    TERMINAL_POINTER_WHEEL_UP,
    TERMINAL_POINTER_WHEEL_DOWN,
    TERMINAL_POINTER_WHEEL_LEFT,     // low bits 2; trackpads and tilt wheels send this
    TERMINAL_POINTER_WHEEL_RIGHT     // low bits 3
} TerminalPointerKind;

typedef enum TerminalPointerButton {
    TERMINAL_BUTTON_NONE = 0,
    TERMINAL_BUTTON_LEFT,
    TERMINAL_BUTTON_MIDDLE,
    TERMINAL_BUTTON_RIGHT
} TerminalPointerButton;

// One normalized pointer event.
//
// SLOT RECORD: TerminalPointerEvent (no behaviour of its own)
typedef struct TerminalPointerEvent {
    TerminalPointerKind kind;     // what happened
    TerminalPointerButton button; // which button, when one applies
    size_t x;                     // 0-based cell column
    size_t y;                     // 0-based cell row
    bool shift;
    bool meta;
    bool ctrl;
} TerminalPointerEvent;

typedef enum TerminalInputState {
    TERMINAL_INPUT_IDLE = 0,     // not inside a sequence
    TERMINAL_INPUT_ESCAPE,       // saw ESC
    TERMINAL_INPUT_CSI,          // saw ESC [
    TERMINAL_INPUT_X10,          // saw ESC [ M, collecting three raw bytes
    TERMINAL_INPUT_SGR_BUTTON,   // collecting the button field
    TERMINAL_INPUT_SGR_X,        // collecting the column field
    TERMINAL_INPUT_SGR_Y         // collecting the row field
} TerminalInputState;

// TERMINAL_INPUT_DIGITS_MAX bounds one numeric field. A fifth digit is refused
// rather than accumulated, so no field can overflow a size_t.
#define TERMINAL_INPUT_DIGITS_MAX 4u
// X10 packs three bytes after ESC [ M.
#define TERMINAL_INPUT_X10_BYTES 3u

typedef struct TerminalInput {
    TerminalInputState state;  // where the byte stream currently is
    size_t rawCount;           // X10 bytes collected so far
    size_t field;              // SGR field under construction: 0 button, 1 x, 2 y
    size_t digits;             // digits seen in the current SGR field
    size_t values[3];          // button, column, row (1-based as received)
    size_t malformed;          // refused sequences in this epoch
    TerminalPointerEvent event; // the last completed event
    bool ready;                // set for the feed() call that completed an event
} TerminalInput;

TerminalInput TerminalInput_0(void);
#define TerminalInput(...) CONSTRUCTOR_DISPATCH(TerminalInput, __VA_ARGS__)
#define TerminalInput_zero() TerminalInput_0()

// Feed one byte. Returns true when that byte completed an event, which is then
// readable with TerminalInput_getEvent. Any other byte returns false; a byte
// inside a sequence never emits.
bool TerminalInput_feed(TerminalInput *self, int byte);

TerminalPointerEvent TerminalInput_getEvent(const TerminalInput *self);
TerminalPointerKind TerminalInput_getKind(const TerminalInput *self);
TerminalPointerButton TerminalInput_getButton(const TerminalInput *self);
bool TerminalInput_isReady(const TerminalInput *self);
// True while a sequence is mid-flight, so a host can tell a partial escape from
// an idle stream (and decide whether to wait for more bytes).
bool TerminalInput_isPending(const TerminalInput *self);
TerminalInputState TerminalInput_getState(const TerminalInput *self);
// Sequences refused in this epoch: a malformed byte, an over-long numeric field,
// or a zero coordinate. A count, never a log line — this runs per byte.
size_t TerminalInput_malformedCount(const TerminalInput *self);
void TerminalInput_clearMalformed(TerminalInput *self);
void TerminalInput_reset(TerminalInput *self);

TerminalProjection TerminalInput_toString(const TerminalInput *self);
TerminalProjection TerminalInput_toStringStruct(const TerminalInput *self);

#endif
