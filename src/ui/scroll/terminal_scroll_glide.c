#include "ui/scroll/terminal_scroll_glide.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/intention.h"
#include "annotation/hotcode.h"
#include <math.h>
#include <stdint.h>

;;DEFINITION
/** A scroll with MOMENTUM, per axis: a position (`rowShown`/`columnShown`), where
 * it is heading (`rowTarget`/`columnTarget`), and the easing between them.
 *
 * A queue that is spent a fixed step at a time is a conveyor belt - it runs at
 * constant speed until it empties, so there is no friction and nothing to stop.
 * This instead covers a fraction of the remaining distance every frame, which
 * decelerates the closer it gets, and `TerminalScrollGlide_halt` is what a press
 * does: it drops the target onto the position and the motion is over at once.
 *
 * The class owns no cells and paints nothing. A view renders at the whole-cell
 * position `TerminalScrollGlide_step` returns; the panel that clips cells and
 * draws them is a separate owner. Keeping the motion here lets every terminal
 * view share the same feel without each one inventing its own easing. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalScrollGlide (ui/scroll/terminal_scroll_glide.c)
 * STRUCT FIELDS (Mirroring ui/scroll/terminal_scroll_glide.h)
 * double rowShown       rows currently displayed, fractional
 * double rowTarget      rows headed for
 * double columnShown    columns currently displayed, fractional
 * double columnTarget   columns headed for
 * double ease           fraction of remaining distance covered per step
 * double epsilon        remaining distance that snaps to the target
 * SLOT RECORD: TerminalScrollGlidePosition
 * size_t row            whole-cell row to paint
 * size_t column         whole-cell column to paint
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalScrollGlide_0(void), _2(ease, epsilon)
 * Config: set/getEase, set/getEpsilon
 * Motion: isActive, impulse, setTarget, setPosition, setRowPosition,
 *   setColumnPosition, halt, clamp, step
 * Getters: getRowShown, getRowTarget, getColumnShown, getColumnTarget,
 *   getPosition
 * Projections: toString, toStringStruct
 * PRIVATE HELPERS
 * static double clampAxis(value, maximum)   clamp one axis into [0, maximum]
 * static double gap(shown, target)           absolute remaining distance
 * static size_t roundAxis(value)             nearest whole cell
 * static double stepAxis(shown, target, ease, epsilon)
 *   one axis of one frame; returns the new shown position
 * CONTRACT: null mutators no-op and null getters return safe defaults. The cold
 * setters/constructors reject invalid ease (must be in (0,1)) and epsilon
 * (must be positive and finite) with one THROW and preserve prior state. step is
 * the frame path: one null guard, no logging, no allocation; it clamps nothing a
 * validated caller already clamped. Positions are never negative, and a saturated
 * double bound converts to SIZE_MAX rather than making an out-of-range cast.
 * ======================================================================== */

static double clampAxis(double value, size_t maximum) {
    double top = (double) maximum;
    if (value < 0.0)
        return 0.0;
    if (value > top)
        return top;
    return value;
}

static double gap(double shown, double target) {
    double difference = target - shown;
    return difference < 0.0 ? -difference : difference;
}

static size_t roundAxis(double value) {
    if (!(value > 0.0))
        return 0u;
    // (double) SIZE_MAX rounds up to 2^64, so a value at or beyond it would make
    // the cast below out of range. Clamp before converting; the public bounds are
    // already size_t, so this only catches a saturated double, never a real cell.
    if (value >= (double) SIZE_MAX)
        return SIZE_MAX;
    return (size_t) (value + 0.5);
}

static double stepAxis(double *shown, double target, double ease, double epsilon) {
    if (gap(*shown, target) < epsilon) {
        *shown = target;
        return target;
    }
    *shown += (target - *shown) * ease;
    return *shown;
}

TerminalScrollGlide TerminalScrollGlide_0(void) {
    TerminalScrollGlide glide = {0};
    glide.ease = TERMINAL_SCROLL_EASE_DEFAULT;
    glide.epsilon = TERMINAL_SCROLL_EPSILON_DEFAULT;
    return glide;
}

TerminalScrollGlide TerminalScrollGlide_2(double ease, double epsilon) {
    TerminalScrollGlide glide = TerminalScrollGlide_0();
    if (!(ease > 0.0 && ease < 1.0)) {
        THROW("TerminalScrollGlide ease: %.6f outside (0,1)", ease);
        return glide;
    }
    if (!(epsilon > 0.0) || !isfinite(epsilon)) {
        THROW("TerminalScrollGlide epsilon: %.6f not positive", epsilon);
        return glide;
    }
    glide.ease = ease;
    glide.epsilon = epsilon;
    return glide;
}

bool TerminalScrollGlide_setEase(TerminalScrollGlide *self, double ease) {
    if (self == nullptr) {
        THROW("TerminalScrollGlide ease: null");
        return false;
    }
    if (!(ease > 0.0 && ease < 1.0)) {
        THROW("TerminalScrollGlide ease: %.6f outside (0,1)", ease);
        return false;
    }
    (*self).ease = ease;
    return true;
}

double TerminalScrollGlide_getEase(const TerminalScrollGlide *self) {
    return self ? (*self).ease : TERMINAL_SCROLL_EASE_DEFAULT;
}

bool TerminalScrollGlide_setEpsilon(TerminalScrollGlide *self, double epsilon) {
    if (self == nullptr) {
        THROW("TerminalScrollGlide epsilon: null");
        return false;
    }
    if (!(epsilon > 0.0) || !isfinite(epsilon)) {
        THROW("TerminalScrollGlide epsilon: %.6f not positive", epsilon);
        return false;
    }
    (*self).epsilon = epsilon;
    return true;
}

double TerminalScrollGlide_getEpsilon(const TerminalScrollGlide *self) {
    return self ? (*self).epsilon : TERMINAL_SCROLL_EPSILON_DEFAULT;
}

bool TerminalScrollGlide_isActive(const TerminalScrollGlide *self) {
    if (self == nullptr)
        return false;
    return gap((*self).rowShown, (*self).rowTarget) >= (*self).epsilon
        || gap((*self).columnShown, (*self).columnTarget) >= (*self).epsilon;
}

void TerminalScrollGlide_impulse(TerminalScrollGlide *self, long rows, long columns,
                                 size_t maxRows, size_t maxColumns) {
    if (self == nullptr)
        return;
    (*self).rowTarget = clampAxis((*self).rowTarget + (double) rows, maxRows);
    (*self).columnTarget = clampAxis((*self).columnTarget + (double) columns, maxColumns);
}

void TerminalScrollGlide_setTarget(TerminalScrollGlide *self, size_t row, size_t column,
                                   size_t maxRows, size_t maxColumns) {
    if (self == nullptr)
        return;
    (*self).rowTarget = clampAxis((double) row, maxRows);
    (*self).columnTarget = clampAxis((double) column, maxColumns);
}

void TerminalScrollGlide_setPosition(TerminalScrollGlide *self, size_t row, size_t column,
                                     size_t maxRows, size_t maxColumns) {
    if (self == nullptr)
        return;
    double clampedRow = clampAxis((double) row, maxRows);
    double clampedColumn = clampAxis((double) column, maxColumns);
    (*self).rowShown = clampedRow;
    (*self).rowTarget = clampedRow;
    (*self).columnShown = clampedColumn;
    (*self).columnTarget = clampedColumn;
}

void TerminalScrollGlide_setRowPosition(TerminalScrollGlide *self, size_t row, size_t maxRows) {
    if (self == nullptr)
        return;
    double clampedRow = clampAxis((double) row, maxRows);
    (*self).rowShown = clampedRow;
    (*self).rowTarget = clampedRow;
}

void TerminalScrollGlide_setColumnPosition(TerminalScrollGlide *self, size_t column,
                                           size_t maxColumns) {
    if (self == nullptr)
        return;
    double clampedColumn = clampAxis((double) column, maxColumns);
    (*self).columnShown = clampedColumn;
    (*self).columnTarget = clampedColumn;
}

void TerminalScrollGlide_halt(TerminalScrollGlide *self) {
    if (self == nullptr)
        return;
    (*self).rowTarget = (*self).rowShown;
    (*self).columnTarget = (*self).columnShown;
}

void TerminalScrollGlide_clamp(TerminalScrollGlide *self, size_t maxRows, size_t maxColumns) {
    if (self == nullptr)
        return;
    (*self).rowTarget = clampAxis((*self).rowTarget, maxRows);
    (*self).rowShown = clampAxis((*self).rowShown, maxRows);
    (*self).columnTarget = clampAxis((*self).columnTarget, maxColumns);
    (*self).columnShown = clampAxis((*self).columnShown, maxColumns);
}

;;INTENTION("step runs once per frame; its success path is a null guard, two comparisons and two arithmetic steps, with no logging, allocation or per-cell revalidation")
;;HOTCODE
TerminalScrollGlidePosition TerminalScrollGlide_step(TerminalScrollGlide *self) {
    TerminalScrollGlidePosition position = {0};
    if (self == nullptr)
        return position;
    double row = stepAxis(&(*self).rowShown, (*self).rowTarget, (*self).ease, (*self).epsilon);
    double column = stepAxis(&(*self).columnShown, (*self).columnTarget, (*self).ease,
                             (*self).epsilon);
    position.row = roundAxis(row);
    position.column = roundAxis(column);
    return position;
}

double TerminalScrollGlide_getRowShown(const TerminalScrollGlide *self) {
    return self ? (*self).rowShown : 0.0;
}

double TerminalScrollGlide_getRowTarget(const TerminalScrollGlide *self) {
    return self ? (*self).rowTarget : 0.0;
}

double TerminalScrollGlide_getColumnShown(const TerminalScrollGlide *self) {
    return self ? (*self).columnShown : 0.0;
}

double TerminalScrollGlide_getColumnTarget(const TerminalScrollGlide *self) {
    return self ? (*self).columnTarget : 0.0;
}

TerminalScrollGlidePosition TerminalScrollGlide_getPosition(const TerminalScrollGlide *self) {
    TerminalScrollGlidePosition position = {0};
    if (self == nullptr)
        return position;
    position.row = roundAxis((*self).rowShown);
    position.column = roundAxis((*self).columnShown);
    return position;
}

TerminalProjection TerminalScrollGlide_toString(const TerminalScrollGlide *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalScrollGlide{row=%.2f->%.2f,column=%.2f->%.2f}",
                                     (*self).rowShown, (*self).rowTarget,
                                     (*self).columnShown, (*self).columnTarget);
}

TerminalProjection TerminalScrollGlide_toStringStruct(const TerminalScrollGlide *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalScrollGlide{rowShown=%.6f,rowTarget=%.6f,columnShown=%.6f,"
        "columnTarget=%.6f,ease=%.6f,epsilon=%.6f}",
        (*self).rowShown, (*self).rowTarget, (*self).columnShown,
        (*self).columnTarget, (*self).ease, (*self).epsilon);
}
