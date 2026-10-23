#ifndef DETERMINANT_TERMINAL_SCROLL_GLIDE_H
#define DETERMINANT_TERMINAL_SCROLL_GLIDE_H

#include <stdbool.h>
#include <stddef.h>

#include "terminal/terminal_projection.h"
#include "c23/constructor.h"

// terminal/terminal_scroll_glide.h — per-axis viewport momentum.
//
// A scroll is two positions (row, column): where the view is shown, where it is
// heading, and the easing between them. Input moves the target; each frame covers
// a fraction of the remaining distance and snaps once the rest is imperceptible.
// The class owns no cells and paints nothing: a view renders at the returned
// whole-cell position. That is the half of a scroll panel that is independent of
// what is being scrolled.

#define TERMINAL_SCROLL_EASE_DEFAULT 0.18
#define TERMINAL_SCROLL_EPSILON_DEFAULT 0.4

typedef struct TerminalScrollGlide {
    double rowShown;      // rows currently displayed, fractional
    double rowTarget;     // rows headed for
    double columnShown;   // columns currently displayed, fractional
    double columnTarget;  // columns headed for
    double ease;          // fraction of remaining distance covered per step
    double epsilon;       // remaining distance that snaps to the target
} TerminalScrollGlide;

// SLOT RECORD: TerminalScrollGlidePosition (no behaviour of its own)
typedef struct TerminalScrollGlidePosition {
    size_t row;
    size_t column;
} TerminalScrollGlidePosition;

TerminalScrollGlide TerminalScrollGlide_0(void);
TerminalScrollGlide TerminalScrollGlide_2(double ease, double epsilon);
#define TerminalScrollGlide(...) CONSTRUCTOR_DISPATCH(TerminalScrollGlide, __VA_ARGS__)
#define TerminalScrollGlide_zero() TerminalScrollGlide_0()

bool TerminalScrollGlide_setEase(TerminalScrollGlide *self, double ease);
double TerminalScrollGlide_getEase(const TerminalScrollGlide *self);
bool TerminalScrollGlide_setEpsilon(TerminalScrollGlide *self, double epsilon);
double TerminalScrollGlide_getEpsilon(const TerminalScrollGlide *self);

// Still travelling? Drives the frame cadence: true while either axis is farther
// than epsilon from its target.
bool TerminalScrollGlide_isActive(const TerminalScrollGlide *self);

// Input moves only the target. Both bounds are inclusive maxima; the target is
// clamped into [0, max] so the glide cannot aim past a bound.
void TerminalScrollGlide_impulse(TerminalScrollGlide *self, long rows, long columns,
                                 size_t maxRows, size_t maxColumns);
void TerminalScrollGlide_setTarget(TerminalScrollGlide *self, size_t row, size_t column,
                                   size_t maxRows, size_t maxColumns);

// Snap one or both axes: both shown and target land on the supplied position(s).
void TerminalScrollGlide_setPosition(TerminalScrollGlide *self, size_t row, size_t column,
                                     size_t maxRows, size_t maxColumns);
void TerminalScrollGlide_setRowPosition(TerminalScrollGlide *self, size_t row, size_t maxRows);
void TerminalScrollGlide_setColumnPosition(TerminalScrollGlide *self, size_t column,
                                           size_t maxColumns);

// Stop where we are: drop the target onto the shown position.
void TerminalScrollGlide_halt(TerminalScrollGlide *self);
// Clamp both shown and target after a layout or extent change.
void TerminalScrollGlide_clamp(TerminalScrollGlide *self, size_t maxRows, size_t maxColumns);

// Advance one frame and return the whole-cell position to paint. Call once per
// frame; a settled glide returns the same position and costs two comparisons.
TerminalScrollGlidePosition TerminalScrollGlide_step(TerminalScrollGlide *self);

double TerminalScrollGlide_getRowShown(const TerminalScrollGlide *self);
double TerminalScrollGlide_getRowTarget(const TerminalScrollGlide *self);
double TerminalScrollGlide_getColumnShown(const TerminalScrollGlide *self);
double TerminalScrollGlide_getColumnTarget(const TerminalScrollGlide *self);
TerminalScrollGlidePosition TerminalScrollGlide_getPosition(const TerminalScrollGlide *self);

TerminalProjection TerminalScrollGlide_toString(const TerminalScrollGlide *self);
TerminalProjection TerminalScrollGlide_toStringStruct(const TerminalScrollGlide *self);

#endif
