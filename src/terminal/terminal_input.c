#include "terminal/terminal_input.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/intention.h"
#include <stdio.h>

;;DEFINITION
/** The mouse escape grammar, decoded once for the whole ecosystem.
 *
 * Every terminal application that wants hover has to do the same three things:
 * read the mouse, understand an escape sequence, and hand the rest of the program
 * a coordinate it can reason about. Primeagen did it by hand inside its terminal
 * loop; Semicolon would have had to do it again. This class is that job moved to
 * the toolkit, where it can be proved without a terminal at all — which is the
 * point, because mouse bugs are miserable to reproduce by hand and trivial to
 * reproduce as a byte string.
 *
 * It is a state machine rather than a parser over a buffer because the host
 * receives a stream one byte at a time from its input call, and a sequence can be
 * split across calls by nothing more than scheduling. Pure and total: no I/O, no
 * allocation, no clock, and no input can overflow a field or wedge the machine —
 * a bad byte returns it to idle and raises a counter.
 *
 * What it deliberately does NOT own: the terminal mode. Whether the terminal
 * sends motion at all, and whether the mouse belongs to the application or to the
 * terminal's own selection, is the host's decision; this decoder reports whatever
 * it is given. That separation is why "disable mouse reporting" and "track the
 * mouse" can each be answered honestly instead of pretending to coexist. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalInput (terminal/terminal_input.c)
 * STRUCT FIELDS (Mirroring terminal/terminal_input.h)
 * TerminalInputState state    where the byte stream currently is
 * size_t rawCount            X10 bytes collected so far
 * size_t field               SGR field under construction (0 button, 1 x, 2 y)
 * size_t digits              digits seen in the current SGR field
 * size_t values[3]           button, column, row as received (1-based)
 * size_t malformed           refused sequences in this epoch
 * TerminalPointerEvent event the last completed event
 * bool ready                 set for the feed() call that completed an event
 * SLOT RECORD: TerminalPointerEvent
 * TerminalPointerKind kind     what happened
 * TerminalPointerButton button which button, when one applies
 * size_t x, y                  0-based cell column and row
 * bool shift, meta, ctrl       modifiers held at the time
 * PRIVATE HELPERS
 * beginSgr(self)              reset the SGR fields and enter the button field
 * noteMalformed(self)         count a refusal, return to idle, set no event
 * buttonFor(low)              low two bits -> TerminalPointerButton
 * complete(self, raw, x, y, release, sgr)  shared decode for both encodings
 * FUNCTION REGISTRY (exports)
 * Constructor: TerminalInput TerminalInput_0(void) [zero/arity chooser]
 * Feed: bool TerminalInput_feed(self, byte)
 * Queries: TerminalPointerEvent getEvent(self), getKind/getButton(self)
 *   bool isReady/isPending(self), TerminalInputState getState(self)
 *   size_t malformedCount(self)
 * Lifecycle: void TerminalInput_reset(self), void clearMalformed(self)
 * Projections: TerminalProjection TerminalInput_toString/toStringStruct(self)
 * CONTRACT: feed is hot and quiet — one range guard, no allocation, no logging,
 * and a malformed byte only increments a counter. Coordinates arrive 1-based and
 * leave 0-based; a zero coordinate is refused because it cannot address a cell.
 * A field longer than TERMINAL_INPUT_DIGITS_MAX is refused, so no value can
 * overflow. A completed event is reported for exactly one feed call. Null self is
 * refused loudly by the cold projections only; feed(null) is a quiet false.
 * ======================================================================== */

static TerminalPointerButton buttonFor(size_t low) {
    switch (low & 3u) {
        case 0: return TERMINAL_BUTTON_LEFT;
        case 1: return TERMINAL_BUTTON_MIDDLE;
        case 2: return TERMINAL_BUTTON_RIGHT;
        default: return TERMINAL_BUTTON_NONE;
    }
}

static void noteMalformed(TerminalInput *self) {
    if ((*self).malformed != SIZE_MAX)
        ++(*self).malformed;
    (*self).state = TERMINAL_INPUT_IDLE;
    (*self).rawCount = 0;
    (*self).field = 0;
    (*self).digits = 0;
}

static void beginSgr(TerminalInput *self) {
    (*self).state = TERMINAL_INPUT_SGR_BUTTON;
    (*self).field = 0;
    (*self).digits = 0;
    (*self).values[0] = 0;
    (*self).values[1] = 0;
    (*self).values[2] = 0;
}

// Shared decode for both encodings. `motion` exists only in SGR, and `release` is
// SGR's lowercase final byte; X10 signals a release by reporting low bits 3.
static void complete(TerminalInput *self, size_t raw, size_t x, size_t y,
                     bool release, bool sgr) {
    if (x == 0 || y == 0) {
        noteMalformed(self);
        return;
    }
    TerminalPointerEvent event = {0};
    event.shift = (raw & 4u) != 0;
    event.meta = (raw & 8u) != 0;
    event.ctrl = (raw & 16u) != 0;
    size_t low = raw & 3u;
    bool motion = sgr && (raw & 32u) != 0;
    bool wheel = (raw & 64u) != 0;
    if (wheel) {
        // 64/65 are vertical; 66/67 are the horizontal pair a trackpad or a tilt
        // wheel sends, and dropping them was why panning sideways was impossible.
        switch (low) {
            case 0: event.kind = TERMINAL_POINTER_WHEEL_UP; break;
            case 1: event.kind = TERMINAL_POINTER_WHEEL_DOWN; break;
            case 2: event.kind = TERMINAL_POINTER_WHEEL_LEFT; break;
            default: event.kind = TERMINAL_POINTER_WHEEL_RIGHT; break;
        }
    } else if (motion) {
        event.kind = low == 3 ? TERMINAL_POINTER_MOVE : TERMINAL_POINTER_DRAG;
    } else if (release || low == 3) {
        event.kind = TERMINAL_POINTER_RELEASE;
    } else {
        event.kind = TERMINAL_POINTER_PRESS;
    }
    event.button = wheel ? TERMINAL_BUTTON_NONE : buttonFor(low);
    if (event.kind == TERMINAL_POINTER_NONE) {
        noteMalformed(self);
        return;
    }
    event.x = x - 1;
    event.y = y - 1;
    (*self).event = event;
    (*self).ready = true;
    (*self).state = TERMINAL_INPUT_IDLE;
    (*self).rawCount = 0;
    (*self).field = 0;
    (*self).digits = 0;
}

TerminalInput TerminalInput_0(void) { return (TerminalInput) {0}; }

;;INTENTION("feed is a hot path: one guard per byte, no logging, no allocation, and a malformed sequence only raises a queryable counter")
bool TerminalInput_feed(TerminalInput *self, int byte) {
    if (self == nullptr || byte < 0 || byte > 255)
        return false;
    (*self).ready = false;
    unsigned char incoming = (unsigned char) byte;
    switch ((*self).state) {
        case TERMINAL_INPUT_IDLE:
            if (incoming == 0x1Bu)
                (*self).state = TERMINAL_INPUT_ESCAPE;
            break;
        case TERMINAL_INPUT_ESCAPE:
            (*self).state = incoming == '[' ? TERMINAL_INPUT_CSI : TERMINAL_INPUT_IDLE;
            break;
        case TERMINAL_INPUT_CSI:
            if (incoming == '<')
                beginSgr(self);
            else if (incoming == 'M') {
                (*self).state = TERMINAL_INPUT_X10;
                (*self).rawCount = 0;
            } else
                noteMalformed(self);
            break;
        case TERMINAL_INPUT_X10:
            if (incoming < 32u)
                noteMalformed(self);
            else {
                (*self).values[(*self).rawCount] = (size_t) incoming - 32u;
                if (++(*self).rawCount == TERMINAL_INPUT_X10_BYTES)
                    complete(self, (*self).values[0], (*self).values[1], (*self).values[2],
                             false, false);
            }
            break;
        case TERMINAL_INPUT_SGR_BUTTON:
        case TERMINAL_INPUT_SGR_X:
        case TERMINAL_INPUT_SGR_Y:
            if (incoming >= '0' && incoming <= '9') {
                if ((*self).digits >= TERMINAL_INPUT_DIGITS_MAX) {
                    noteMalformed(self);
                    break;
                }
                (*self).values[(*self).field] =
                    (*self).values[(*self).field] * 10u + (size_t) (incoming - '0');
                ++(*self).digits;
            } else if (incoming == ';' && (*self).field < 2 && (*self).digits > 0) {
                ++(*self).field;
                (*self).digits = 0;
                (*self).state = (*self).field == 1 ? TERMINAL_INPUT_SGR_X : TERMINAL_INPUT_SGR_Y;
            } else if ((incoming == 'M' || incoming == 'm') && (*self).field == 2
                       && (*self).digits > 0) {
                complete(self, (*self).values[0], (*self).values[1], (*self).values[2],
                         incoming == 'm', true);
            } else
                noteMalformed(self);
            break;
        default:
            noteMalformed(self);
            break;
    }
    return (*self).ready;
}

TerminalPointerEvent TerminalInput_getEvent(const TerminalInput *self) {
    return self != nullptr ? (*self).event : (TerminalPointerEvent) {0};
}
TerminalPointerKind TerminalInput_getKind(const TerminalInput *self) {
    if (self == nullptr) return TERMINAL_POINTER_NONE;
    TerminalPointerEvent event = (*self).event;
    return event.kind;
}
TerminalPointerButton TerminalInput_getButton(const TerminalInput *self) {
    if (self == nullptr) return TERMINAL_BUTTON_NONE;
    TerminalPointerEvent event = (*self).event;
    return event.button;
}
bool TerminalInput_isReady(const TerminalInput *self) { return self != nullptr && (*self).ready; }
bool TerminalInput_isPending(const TerminalInput *self) {
    return self != nullptr && (*self).state != TERMINAL_INPUT_IDLE;
}
TerminalInputState TerminalInput_getState(const TerminalInput *self) {
    return self != nullptr ? (*self).state : TERMINAL_INPUT_IDLE;
}
size_t TerminalInput_malformedCount(const TerminalInput *self) {
    return self != nullptr ? (*self).malformed : 0;
}
void TerminalInput_clearMalformed(TerminalInput *self) {
    if (self != nullptr)
        (*self).malformed = 0;
}

void TerminalInput_reset(TerminalInput *self) {
    if (self == nullptr) {
        THROW("TerminalInput reset: null input");
        return;
    }
    size_t malformed = (*self).malformed;
    *self = TerminalInput_0();
    (*self).malformed = malformed;   // reset reopens the decode, not the evidence
}

static const char *kindName(TerminalPointerKind kind) {
    switch (kind) {
        case TERMINAL_POINTER_MOVE: return "move";
        case TERMINAL_POINTER_DRAG: return "drag";
        case TERMINAL_POINTER_PRESS: return "press";
        case TERMINAL_POINTER_RELEASE: return "release";
        case TERMINAL_POINTER_WHEEL_UP: return "wheel-up";
        case TERMINAL_POINTER_WHEEL_DOWN: return "wheel-down";
        case TERMINAL_POINTER_WHEEL_LEFT: return "wheel-left";
        case TERMINAL_POINTER_WHEEL_RIGHT: return "wheel-right";
        default: return "none";
    }
}

TerminalProjection TerminalInput_toString(const TerminalInput *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    const TerminalPointerEvent *event = &(*self).event;
    return TerminalProjection_format("TerminalInput{%s,button=%d,%zu,%zu,mods=%s%s%s,malformed=%zu}",
        kindName((*event).kind), (*event).button, (*event).x, (*event).y,
        (*event).shift ? "s" : "", (*event).meta ? "m" : "", (*event).ctrl ? "c" : "",
        (*self).malformed);
}

TerminalProjection TerminalInput_toStringStruct(const TerminalInput *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    const TerminalPointerEvent *event = &(*self).event;
    return TerminalProjection_format(
        "TerminalInput{state=%d,rawCount=%zu,field=%zu,digits=%zu,values={%zu,%zu,%zu},malformed=%zu,event={kind=%s,button=%d,x=%zu,y=%zu,shift=%d,meta=%d,ctrl=%d},ready=%d}",
        (*self).state, (*self).rawCount, (*self).field, (*self).digits,
        (*self).values[0], (*self).values[1], (*self).values[2], (*self).malformed,
        kindName((*event).kind), (*event).button, (*event).x, (*event).y,
        (*event).shift, (*event).meta, (*event).ctrl, (*self).ready);
}
