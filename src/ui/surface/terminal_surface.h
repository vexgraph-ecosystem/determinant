#ifndef DETERMINANT_TERMINAL_SURFACE_H
#define DETERMINANT_TERMINAL_SURFACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"
#include "c23/constructor.h"

// ui/surface/terminal_surface.h — the LAST-PAINT presenter.
//
// A TerminalSurface owns no memory: the caller lends TWO canvases over the SAME
// geometry — `back`, which the frame is composed into, and `front`, the record of
// what the host was last told. TerminalSurface_present diffs them and hands the
// host only the cells that actually changed, as contiguous same-row RUNS through
// the injected TerminalRunFn. Static content therefore costs nothing: a frame
// that composes the same cells again emits zero runs and the host may skip its
// device flush entirely. This is the "stamp what changed since the last paint"
// step, and it is where the four-plane canvas finally reaches a host sink.
//
// The seam stays host-neutral: cells are codepoints, styles are the
// TERMINAL_STYLE_* flags, colours are packed 0xRRGGBBAA. Nothing here names a
// terminal, emits an escape, allocates, or writes to a device — the host maps a
// run into ncurses, ANSI, a Windows console or a cell GPU.
//
// `front` is the caller's memory, never an internal cache: rebinding it after a
// resize, or after the host lost the device, is how a host resynchronises (see
// TerminalSurface_reset). Adoption of a successful present is all-or-nothing — a
// sink that refuses part-way leaves `front` untouched, so the next present
// re-delivers the same delta (idempotent) rather than dropping a change.

// A contiguous SAME-ROW window of the grid: `length` cells starting at absolute
// (x, y). Every plane the surface has bound is non-null and points at the run's
// FIRST cell; the run's remaining cells follow at +1, because a single row is
// contiguous in the planes' row-major layout — no stride is needed.
//
// SLOT RECORD: TerminalRun (no behaviour of its own; owned by TerminalSurface)
typedef struct TerminalRun {
    size_t x;               // absolute left column
    size_t y;               // absolute row
    size_t length;          // cells in this run (>= 1)
    const uint32_t *cells;  // borrowed glyph codepoints at the first cell
    const uint8_t *styles;  // borrowed style flags at the first cell (nullable)
    const uint32_t *fg;     // borrowed foreground at the first cell (nullable)
    const uint32_t *bg;     // borrowed background at the first cell (nullable)
} TerminalRun;

// A present is a TRANSACTION. The host is told when one opens and when it closes, so
// a device that can apply an update atomically does: every changed run belongs to one
// frame, and a frame is published as a unit. This is the terminal's counterpart of a
// compositor transaction - the same discipline the native stack applies to a layer,
// where all of a frame's changes land in one commit rather than dribbling out.
//
// The bracket is opened LAZILY, immediately before the first changed run, so a frame
// that changes nothing opens no transaction and stays completely silent (the
// Present-On-Demand rule). It is closed even when a sink refused part-way, because a
// device left inside a half-finished transaction is a device left frozen.
//
// The phases are notifications, not requests: what the bracket MEANS belongs to the
// host's device. A terminal maps it onto synchronized output; a layer-backed surface
// would map it onto its own commit.
typedef enum TerminalPresentPhase {
    TERMINAL_PRESENT_BEGIN = 0,  // a transaction is open; the changed runs follow
    TERMINAL_PRESENT_END         // the transaction closes; the host may publish
} TerminalPresentPhase;

// The frame bracket. Called at most once per phase per present, and never at all for
// a frame that changed nothing.
typedef void (*TerminalFrameFn)(void *frameContext, TerminalPresentPhase phase);

// The host sink: accept ONE changed run. Returning false refuses it, which the
// surface records and counts (the Exhaustion Loudness Law) and re-delivers on the
// next present. Bounded and synchronous; the planes are borrowed for the duration
// of the call only, and the sink must NOT reenter the surface.
typedef bool (*TerminalRunFn)(void *hostContext, TerminalRun run);

// The outcome of one present, as a value (the Value Return Law).
//
// SLOT RECORD: TerminalPresentResult (no behaviour of its own)
typedef struct TerminalPresentResult {
    bool ok;             // every changed run was accepted
    bool changed;        // any cell differed from the last paint
    bool transactional;  // a frame transaction was opened AND closed around the runs
    size_t changedCells; // differing cells found by this present's scan
    size_t runs;         // runs handed to the sink
    size_t emittedCells; // cells inside those runs (>= changedCells)
} TerminalPresentResult;

// Gap tolerance: the largest run of unchanged cells allowed INSIDE one emitted
// run before the surface splits it. 0 emits the tightest possible runs; the
// default 1 joins two changes one cell apart, so a single space between two
// changed words does not cost a second sink call. Re-sending an unchanged cell
// inside a run is idempotent, so tolerance changes batching, never content, and
// there is deliberately no ceiling on it (a huge value can only widen runs).
#define TERMINAL_SURFACE_GAP_DEFAULT 1u

typedef struct TerminalSurface {
    TerminalCanvas back;      // composed-next grid (borrowed planes)
    TerminalCanvas front;     // last-presented grid (borrowed planes)
    TerminalRunFn sink;       // borrowed host run sink
    void *hostContext;        // opaque resident sink state
    TerminalFrameFn frame;    // borrowed frame bracket (nullable)
    void *frameContext;       // opaque bracket state
    bool bracketed;           // a transaction is currently open
    size_t gapTolerance;      // max unchanged run inside one emitted run
    size_t presentCount;      // completed presents (lifetime)
    size_t changedCells;      // differing cells in the last present
    size_t changedRuns;       // runs handed to the sink in the last present
    size_t emittedCells;      // cells inside those runs in the last present
    size_t refusalCount;      // refusals in this epoch
    bool active;              // sink in flight (reentrancy guard)
} TerminalSurface;

TerminalSurface TerminalSurface_0(void);
TerminalSurface TerminalSurface_2(TerminalCanvas back, TerminalCanvas front);
#define TerminalSurface(...) CONSTRUCTOR_DISPATCH(TerminalSurface, __VA_ARGS__)
#define TerminalSurface_zero() TerminalSurface_0()

// Cold admission. Both canvases must be valid, over IDENTICAL geometry, with the
// SAME planes present (so a difference can never be silently invisible), and the
// sink non-null. `front` is adopted as the last paint exactly as it stands and
// the refusal epoch starts fresh; call TerminalSurface_reset to declare instead
// that the host has never painted.
bool TerminalSurface_bind(TerminalSurface *self, const TerminalCanvas *back,
                          const TerminalCanvas *front, TerminalRunFn sink, void *hostContext);
bool TerminalSurface_setSink(TerminalSurface *self, TerminalRunFn sink, void *hostContext);
// Install the frame bracket. Refused while a transaction is open, for the same reason
// the sink is: the host must not change who is being told mid-frame.
bool TerminalSurface_setFrame(TerminalSurface *self, TerminalFrameFn frame, void *frameContext);
TerminalFrameFn TerminalSurface_getFrame(const TerminalSurface *self);
void *TerminalSurface_getFrameContext(const TerminalSurface *self);
bool TerminalSurface_isBracketed(const TerminalSurface *self);
bool TerminalSurface_setGapTolerance(TerminalSurface *self, size_t tolerance);
size_t TerminalSurface_getGapTolerance(const TerminalSurface *self);

// Hot: emit only what differs, then adopt `back` as the new front. Quiet on
// success — no logging and no allocation on a clean frame, and an unchanged frame
// hands the sink nothing at all. The failing path reports at most once per epoch
// and counts every refusal.
TerminalPresentResult TerminalSurface_present(TerminalSurface *self);

// Cold: declare the host's last paint equal to the current `back` (the first
// paint after bind, or after the host resynchronised its device), and start a
// fresh refusal epoch.
bool TerminalSurface_reset(TerminalSurface *self);

// Queries (safe defaults on null).
bool TerminalSurface_isBound(const TerminalSurface *self);
bool TerminalSurface_isActive(const TerminalSurface *self);
TerminalCanvas TerminalSurface_getBack(const TerminalSurface *self);
TerminalCanvas TerminalSurface_getFront(const TerminalSurface *self);
size_t TerminalSurface_presentCount(const TerminalSurface *self);
size_t TerminalSurface_changedCells(const TerminalSurface *self);
size_t TerminalSurface_changedRuns(const TerminalSurface *self);
size_t TerminalSurface_emittedCells(const TerminalSurface *self);
size_t TerminalSurface_exhaustionCount(const TerminalSurface *self);

TerminalProjection TerminalSurface_toString(const TerminalSurface *self);
TerminalProjection TerminalSurface_toStringStruct(const TerminalSurface *self);

#endif
