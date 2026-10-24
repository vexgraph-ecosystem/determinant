#include "terminal/terminal_surface.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/intention.h"
#include "annotation/hotcode.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** The last-paint presenter. TerminalSurface diffs the frame the application just
 * composed (`back`) against the record of what the host was last told (`front`)
 * and hands over only the cells that differ, as contiguous same-row runs. It
 * exists because the four-plane canvas had no way to reach a host sink, and
 * because re-sending an unchanged screen every frame is the terminal's version of
 * re-rendering a clean scene: it costs the device a full repaint, it flickers on
 * slow links, and it destroys the cheap "nothing changed, do nothing" answer that
 * a static interface deserves. The surface owns no memory — both grids are lent
 * by the caller, so a resize is a rebind rather than an internal reallocation —
 * and it never names a terminal or emits an escape: a run is cells, style flags
 * and packed colours, and the host maps it to ncurses, ANSI or a console. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalSurface (terminal/terminal_surface.c)
 * STRUCT FIELDS (Mirroring terminal/terminal_surface.h)
 * TerminalCanvas back            composed-next grid (borrowed planes)
 * TerminalCanvas front           last-presented grid (borrowed planes)
 * TerminalRunFn sink             borrowed host run sink
 * void *hostContext              opaque resident sink state
 * TerminalFrameFn frame          borrowed frame bracket (nullable)
 * void *frameContext             opaque bracket state
 * bool bracketed                 a transaction is currently open
 * size_t gapTolerance            max unchanged run inside one emitted run
 * size_t presentCount            completed presents (lifetime)
 * size_t changedCells            differing cells in the last present
 * size_t changedRuns             runs handed to the sink in the last present
 * size_t emittedCells            cells inside those runs in the last present
 * size_t refusalCount            refusals in this epoch
 * bool active                    sink in flight (reentrancy guard)
 * SLOT RECORD: TerminalRun
 * size_t x, y, length            absolute origin and cell count of one run
 * const uint32_t *cells          borrowed glyph codepoints at the first cell
 * const uint8_t *styles          borrowed style flags at the first cell (nullable)
 * const uint32_t *fg, *bg        borrowed packed colours at the first cell (nullable)
 * SLOT RECORD: TerminalPresentResult
 * bool ok                        every changed run was accepted
 * bool changed                   any cell differed from the last paint
 * bool transactional             a frame transaction opened AND closed
 * size_t changedCells            differing cells found by the scan
 * size_t runs                    runs handed to the sink
 * size_t emittedCells            cells inside those runs
 * PRIVATE HELPERS
 * sameGeometry(a,b)      cold admission identity: grid AND region must match
 * samePlanes(a,b)        cold admission: style/colour plane presence must match
 * cellIndex(canvas,x,y)  one row-major index, honouring the region origin
 * cellDiffers(back,front,index)  one bound comparison across the present planes
 * emitRun(self,y,start,last)     hand one same-row run to the sink
 * adoptFront(self)               region copy back -> front after a present
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalSurface_0(void), _2(back, front) [zero/arity chooser]
 * Admission: bool TerminalSurface_bind(self, back, front, sink, hostContext)
 *   bool TerminalSurface_setSink(self, sink, hostContext)
 *   bool TerminalSurface_reset(self)
 *   bool setFrame/getFrame/getFrameContext/isBracketed(self, ...)
 * Configuration: bool setGapTolerance(self, tolerance), size_t getGapTolerance(self)
 * Present: TerminalPresentResult TerminalSurface_present(self)
 * Queries: bool isBound/isActive(self)
 *   TerminalCanvas getBack/getFront(self)
 *   size_t presentCount/changedCells/changedRuns/emittedCells/exhaustionCount(self)
 * Projections: TerminalProjection TerminalSurface_toString/toStringStruct(self)
 * CONTRACT: null/invalid surfaces are safe (queries answer defaults, present
 * refuses); geometry and plane presence are validated once at the cold bind, so
 * the scan carries a single entry guard and never revalidates per cell. Out-of-
 * region cells are never touched. The scan allocates nothing, logs nothing, and
 * a refused sink reports at most ONE diagnostic per epoch while refusalCount
 * counts every refusal; bind and reset open a fresh epoch. A sink failure leaves
 * `front` untouched, so the next present re-delivers the same delta, and only a
 * completely accepted present adopts `back` as the new front. All calls require
 * host external serialization; the sink must not reenter the surface.
 * ======================================================================== */

static bool sameGeometry(const TerminalCanvas *a, const TerminalCanvas *b) {
    return (*a).gridWidth == (*b).gridWidth && (*a).gridHeight == (*b).gridHeight
        && (*a).x == (*b).x && (*a).y == (*b).y
        && (*a).width == (*b).width && (*a).height == (*b).height;
}

static bool samePlanes(const TerminalCanvas *a, const TerminalCanvas *b) {
    return ((*a).styles != nullptr) == ((*b).styles != nullptr)
        && ((*a).fg != nullptr) == ((*b).fg != nullptr);
}

static size_t cellIndex(const TerminalCanvas *canvas, size_t x, size_t y) {
    return (y + (*canvas).y) * (*canvas).gridWidth + (*canvas).x + x;
}

// One index, compared across every plane the pair actually carries. A plane the
// canvases do not carry is not a dimension of difference, which is exactly why
// bind demands matching presence: a difference can never be invisible.
static bool cellDiffers(const TerminalCanvas *back, const TerminalCanvas *front, size_t index) {
    if ((*back).cells[index] != (*front).cells[index])
        return true;
    if ((*back).styles != nullptr && (*back).styles[index] != (*front).styles[index])
        return true;
    if ((*back).fg != nullptr
        && ((*back).fg[index] != (*front).fg[index] || (*back).bg[index] != (*front).bg[index]))
        return true;
    return false;
}

static bool emitRun(TerminalSurface *self, size_t y, size_t start, size_t last) {
    // The transaction opens on the first run that is actually going to be handed over,
    // so a frame carrying no change never opens one.
    if (!(*self).bracketed && (*self).frame != nullptr) {
        (*self).bracketed = true;
        (*self).frame((*self).frameContext, TERMINAL_PRESENT_BEGIN);
    }
    const TerminalCanvas *back = &(*self).back;
    size_t gridY = y + (*back).y;
    size_t first = cellIndex(back, start, y);
    size_t length = last - start + 1;
    TerminalRun run = {0};
    run.x = (*back).x + start;
    run.y = gridY;
    run.length = length;
    run.cells = &(*back).cells[first];
    if ((*back).styles != nullptr)
        run.styles = &(*back).styles[first];
    if ((*back).fg != nullptr) {
        run.fg = &(*back).fg[first];
        run.bg = &(*back).bg[first];
    }
    ++(*self).changedRuns;
    (*self).emittedCells += length;
    if ((*self).sink((*self).hostContext, run))
        return true;
    if ((*self).refusalCount == 0)
        THROW("TerminalSurface exhausted: sink refused run at %zu,%zu (length %zu, bound %zux%zu)",
              run.x, run.y, run.length, (*back).width, (*back).height);
    if ((*self).refusalCount != SIZE_MAX)
        ++(*self).refusalCount;
    return false;
}

// Copy the region from back to front, one row at a time so cells outside the
// region are never touched. Styles and colours are guarded SEPARATELY: a canvas
// may carry a style plane and no colour planes at all.
static void adoptFront(TerminalSurface *self) {
    const TerminalCanvas *back = &(*self).back;
    TerminalCanvas *front = &(*self).front;
    size_t width = (*back).width;
    for (size_t y = 0; y < (*back).height; ++y) {
        size_t origin = (*back).x + (y + (*back).y) * (*back).gridWidth;
        memcpy(&(*front).cells[origin], &(*back).cells[origin], width * sizeof(uint32_t));
        if ((*back).styles != nullptr)
            memcpy(&(*front).styles[origin], &(*back).styles[origin], width * sizeof(uint8_t));
        if ((*back).fg != nullptr) {
            memcpy(&(*front).fg[origin], &(*back).fg[origin], width * sizeof(uint32_t));
            memcpy(&(*front).bg[origin], &(*back).bg[origin], width * sizeof(uint32_t));
        }
    }
}

TerminalSurface TerminalSurface_0(void) {
    return (TerminalSurface) { .gapTolerance = TERMINAL_SURFACE_GAP_DEFAULT };
}

TerminalSurface TerminalSurface_2(TerminalCanvas back, TerminalCanvas front) {
    TerminalSurface self = TerminalSurface_0();
    self.back = back;
    self.front = front;
    return self;
}

bool TerminalSurface_bind(TerminalSurface *self, const TerminalCanvas *back,
                          const TerminalCanvas *front, TerminalRunFn sink, void *hostContext) {
    if (self == nullptr || back == nullptr || front == nullptr || sink == nullptr
        || !TerminalCanvas_isValid(back) || !TerminalCanvas_isValid(front)
        || !sameGeometry(back, front) || !samePlanes(back, front) || (*self).active) {
        THROW("TerminalSurface bind: invalid canvases/sink, mismatched geometry or planes, or busy");
        return false;
    }
    (*self).back = *back;
    (*self).front = *front;
    (*self).sink = sink;
    (*self).hostContext = hostContext;
    (*self).presentCount = 0;
    (*self).changedCells = 0;
    (*self).changedRuns = 0;
    (*self).emittedCells = 0;
    (*self).refusalCount = 0;
    return true;
}

bool TerminalSurface_setFrame(TerminalSurface *self, TerminalFrameFn frame, void *frameContext) {
    if (self == nullptr || frame == nullptr || (*self).active || (*self).bracketed) {
        THROW("TerminalSurface setFrame: null bracket, or a transaction is open");
        return false;
    }
    (*self).frame = frame;
    (*self).frameContext = frameContext;
    return true;
}
TerminalFrameFn TerminalSurface_getFrame(const TerminalSurface *self) { return self ? (*self).frame : nullptr; }
void *TerminalSurface_getFrameContext(const TerminalSurface *self) { return self ? (*self).frameContext : nullptr; }
bool TerminalSurface_isBracketed(const TerminalSurface *self) { return self != nullptr && (*self).bracketed; }

bool TerminalSurface_setSink(TerminalSurface *self, TerminalRunFn sink, void *hostContext) {
    if (self == nullptr || sink == nullptr || (*self).active) {
        THROW("TerminalSurface setSink: null sink or busy");
        return false;
    }
    (*self).sink = sink;
    (*self).hostContext = hostContext;
    return true;
}

bool TerminalSurface_setGapTolerance(TerminalSurface *self, size_t tolerance) {
    if (self == nullptr || (*self).active) {
        THROW("TerminalSurface gapTolerance: null surface or busy");
        return false;
    }
    (*self).gapTolerance = tolerance;
    return true;
}

size_t TerminalSurface_getGapTolerance(const TerminalSurface *self) {
    return self ? (*self).gapTolerance : TERMINAL_SURFACE_GAP_DEFAULT;
}

bool TerminalSurface_isBound(const TerminalSurface *self) {
    return self != nullptr && (*self).sink != nullptr
        && TerminalCanvas_isValid(&(*self).back) && TerminalCanvas_isValid(&(*self).front);
}

bool TerminalSurface_isActive(const TerminalSurface *self) {
    return self != nullptr && (*self).active;
}

TerminalCanvas TerminalSurface_getBack(const TerminalSurface *self) {
    return self ? (*self).back : TerminalCanvas_0();
}

TerminalCanvas TerminalSurface_getFront(const TerminalSurface *self) {
    return self ? (*self).front : TerminalCanvas_0();
}

size_t TerminalSurface_presentCount(const TerminalSurface *self) { return self ? (*self).presentCount : 0; }
size_t TerminalSurface_changedCells(const TerminalSurface *self) { return self ? (*self).changedCells : 0; }
size_t TerminalSurface_changedRuns(const TerminalSurface *self) { return self ? (*self).changedRuns : 0; }
size_t TerminalSurface_emittedCells(const TerminalSurface *self) { return self ? (*self).emittedCells : 0; }
size_t TerminalSurface_exhaustionCount(const TerminalSurface *self) { return self ? (*self).refusalCount : 0; }

;;INTENTION("present is a hot path; per the Exhaustion Loudness Law's failure-only waiver its success path adds no logging, allocation or per-cell revalidation, and only a refused run reports — at most once in the epoch")
;;HOTCODE
TerminalPresentResult TerminalSurface_present(TerminalSurface *self) {
    TerminalPresentResult result = {0};
    if (self == nullptr) {
        THROW("TerminalSurface present: null surface");
        return result;
    }
    if (!TerminalSurface_isBound(self) || (*self).active) {
        if ((*self).refusalCount == 0)
            THROW("TerminalSurface refused present: unbound or reentrant");
        if ((*self).refusalCount != SIZE_MAX)
            ++(*self).refusalCount;
        return result;
    }
    const TerminalCanvas *back = &(*self).back;
    const TerminalCanvas *front = &(*self).front;
    size_t width = (*back).width;
    size_t height = (*back).height;
    size_t tolerance = (*self).gapTolerance;
    (*self).changedCells = 0;
    (*self).changedRuns = 0;
    (*self).emittedCells = 0;
    (*self).active = true;
    size_t changed = 0;
    bool accepted = true;
    for (size_t y = 0; y < height && accepted; ++y) {
        size_t start = 0;
        size_t last = 0;
        bool open = false;
        for (size_t x = 0; x < width; ++x) {
            if (cellDiffers(back, front, cellIndex(back, x, y))) {
                if (!open) {
                    start = x;
                    open = true;
                }
                last = x;
                ++changed;
                continue;
            }
            if (!open || x - last <= tolerance)
                continue;
            open = false;
            if (!emitRun(self, y, start, last)) {
                accepted = false;
                break;
            }
        }
        if (accepted && open)
            accepted = emitRun(self, y, start, last);
    }
    // Close the transaction BEFORE anything else is reported. It closes even when a
    // sink refused part-way: leaving a device inside a half-finished transaction is
    // how a screen ends up frozen.
    if ((*self).bracketed) {
        (*self).bracketed = false;
        (*self).frame((*self).frameContext, TERMINAL_PRESENT_END);
        result.transactional = true;
    }
    (*self).active = false;
    (*self).changedCells = changed;
    result.changedCells = changed;
    result.runs = (*self).changedRuns;
    result.emittedCells = (*self).emittedCells;
    result.changed = changed != 0;
    if (accepted) {
        // An unchanged frame needs no adopt: front already equals back, so an
        // idle frame costs one scan and zero writes.
        if (result.runs > 0)
            adoptFront(self);
        ++(*self).presentCount;
        result.ok = true;
    }
    return result;
}

bool TerminalSurface_reset(TerminalSurface *self) {
    if (self == nullptr || !TerminalSurface_isBound(self) || (*self).active) {
        THROW("TerminalSurface reset: unbound or busy");
        return false;
    }
    adoptFront(self);
    (*self).refusalCount = 0;
    (*self).changedCells = 0;
    (*self).changedRuns = 0;
    (*self).emittedCells = 0;
    return true;
}

TerminalProjection TerminalSurface_toString(const TerminalSurface *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalSurface{presented=%zu,changed=%zu,runs=%zu,emitted=%zu,bound=%d,active=%d}",
        (*self).presentCount, (*self).changedCells, (*self).changedRuns, (*self).emittedCells,
        TerminalSurface_isBound(self), (*self).active);
}

TerminalProjection TerminalSurface_toStringStruct(const TerminalSurface *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    TerminalCanvas back = (*self).back;
    TerminalCanvas front = (*self).front;
    return TerminalProjection_format(
        "TerminalSurface{back=%zux%zu@%zu,%zu,front=%zux%zu@%zu,%zu,sink=%s,gapTolerance=%zu,presentCount=%zu,changedCells=%zu,changedRuns=%zu,emittedCells=%zu,refusalCount=%zu,active=%d}",
        back.width, back.height, back.x, back.y,
        front.width, front.height, front.x, front.y,
        (*self).sink ? "bound" : "none", (*self).gapTolerance,
        (*self).presentCount, (*self).changedCells, (*self).changedRuns, (*self).emittedCells,
        (*self).refusalCount, (*self).active);
}
