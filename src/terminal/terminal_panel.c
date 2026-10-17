#include "terminal/terminal_panel.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/intention.h"
#include "annotation/hotcode.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** The layered panel: a stack of anchored layers over one cell region, and the
 * only thing in the toolkit that decides z-order and placement flow. It exists
 * because a terminal interface is a pile of bands — a title, a rail, a centre, a
 * status line, an overlay or two — and every one of them needs the same three
 * answers: where am I, what colour am I, and what paints me. Answering those in
 * one place is what stops each screen growing a private layout pass, and the
 * reserve rule in TerminalAnchor is what lets them compose in plain insertion
 * order with no dirty flags and no second geometry ruleset.
 *
 * The panel owns exactly one thing: its growable slot array. It owns no content,
 * no child panels and no images — a layer borrows an image, borrows a painter,
 * and hands back a sub-canvas. Nesting is therefore free of ownership questions:
 * a layer whose painter calls TerminalPanel_paint(child, &region) is a nested
 * panel, and the application still owns every panel it created.
 *
 * Paint is TOTAL by design. Every visible layer repaints on every call, with no
 * caching and no skipping, because a full repaint is predictable, deterministic
 * and trivially testable; the economy is bought at the presentation boundary
 * instead, where TerminalSurface diffs the painted grid against the last paint so
 * the host receives only what changed. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalPanel (terminal/terminal_panel.c)
 * STRUCT FIELDS (Mirroring terminal/terminal_panel.h)
 * TerminalLayer *layers           owned growable slot array
 * size_t capacity                 admitted slots
 * size_t usedCount                live layers
 * size_t paintCount               completed paints
 * size_t refusalCount             refusals in this epoch
 * bool painting                   a paint is in flight (reentrancy guard)
 * SLOT RECORD: TerminalLayer
 * bool used                       slot holds a live layer
 * uint32_t generation             bumped on slot reuse, so a stale id is detectable
 * bool visible                    hidden layers neither draw nor reserve
 * TerminalAnchor anchor           placement in the parent
 * TerminalPen pen                 layer default style/foreground/background
 * const TerminalImage *image      borrowed backdrop/overlay (nullable)
 * TerminalLayerFit fit            how the image fills its region
 * TerminalLayerDrawFn draw        borrowed content painter (nullable)
 * void *context                   opaque painter context
 * SLOT RECORD: TerminalLayerId
 * size_t index                    slot index
 * uint32_t generation             the generation expected at that slot (0 = invalid)
 * PRIVATE HELPERS
 * TERMINAL_PANEL_NO_SLOT        sentinel index meaning "no live layer here"
 * invalidId()                   the zero id every refusal returns
 * slotFor(self, id)             validate id -> slot, or NO_SLOT when unknown/stale
 * refuse(self, reason)          count a refusal, reporting once per epoch
 * growLayers(self)              cold doubling growth preserving every live layer
 * paintLayerImage(layer, region) borrowed image through its declared fit
 * FUNCTION REGISTRY (exports)
 * Constructor: TerminalPanel *TerminalPanel_0(void) [zero/arity chooser]
 * Lifecycle: void TerminalPanel_free(self)
 * Slots: TerminalLayerId TerminalPanel_add(self, anchor)
 *   bool TerminalPanel_remove(self, id)
 * Configuration: bool setAnchor/getAnchor, setBackgroundColor/getBackgroundColor,
 *   setForegroundColor/getForegroundColor, setStyle/getStyle,
 *   setVisible/isVisible, setImage/getImage/getFit, setDraw/getDraw/getContext
 * Paint: void TerminalPanel_paint(self, canvas)
 * Queries: TerminalRect TerminalPanel_regionOf(self, canvas, id)
 *   size_t layerCount/capacity/paintCount/exhaustionCount(self)
 *   bool isValid(self, id), isPainting(self), isLayerId(id)
 *   bool TerminalLayerId_equal(a, b)
 *   void TerminalPanel_clearExhaustion(self)
 * Projections: TerminalProjection TerminalPanel_toString/toStringStruct(self)
 * CONTRACT: cold operations validate the id and reject loudly without mutating
 * anything — an unknown, stale or retired id changes no layer. Paint is total and
 * hot: one entry guard, no allocation, no logging, no per-cell validation, and a
 * reentrant paint is refused, counted and reported at most once per epoch. A
 * hidden layer neither draws nor reserves. Slot order is z-order and flow order.
 * Layers borrow their image and painter; the panel frees only its own array.
 * Free and structural changes are refused mid-paint. A stale id addresses nothing
 * rather than a recycled layer. Host external serialization is required.
 * ======================================================================== */

#define TERMINAL_PANEL_NO_SLOT ((size_t) -1)

static TerminalLayerId invalidId(void) { return (TerminalLayerId) { 0, 0 }; }

static void refuse(TerminalPanel *self, const char *reason) {
    if ((*self).refusalCount == 0)
        THROW("TerminalPanel exhausted: %s (capacity %zu, live %zu)",
              reason, (*self).capacity, (*self).usedCount);
    if ((*self).refusalCount != SIZE_MAX)
        ++(*self).refusalCount;
}

// Resolve an id to a live slot. Zero generation, out-of-range index, a retired
// slot and a generation mismatch are all "no such layer" — never a silent
// substitute for a recycled one.
static size_t slotFor(const TerminalPanel *self, TerminalLayerId id) {
    if (self == nullptr || id.generation == 0 || id.index >= (*self).capacity)
        return TERMINAL_PANEL_NO_SLOT;
    const TerminalLayer *layer = &(*self).layers[id.index];
    if (!(*layer).used || (*layer).generation != id.generation)
        return TERMINAL_PANEL_NO_SLOT;
    return id.index;
}

// Cold doubling growth. Every live layer survives; a failed allocation leaves the
// panel exactly as it was and is reported through the refusal channel.
static bool growLayers(TerminalPanel *self) {
    size_t next = (*self).capacity == 0 ? TERMINAL_PANEL_LAYERS_DEFAULT : (*self).capacity * 2;
    if (next < (*self).capacity || next > SIZE_MAX / sizeof(TerminalLayer)) {
        refuse(self, "layer capacity overflow");
        return false;
    }
    TerminalLayer *grown = Memory_alloc(TYPE_BYTE_ARRAY, next * sizeof(TerminalLayer));
    if (grown == nullptr) {
        refuse(self, "layer array allocation refused");
        return false;
    }
    if ((*self).capacity != 0)
        memcpy(grown, (*self).layers, (*self).capacity * sizeof(TerminalLayer));
    memset(grown + (*self).capacity, 0, (next - (*self).capacity) * sizeof(TerminalLayer));
    Memory_free((*self).layers);
    (*self).layers = grown;
    (*self).capacity = next;
    return true;
}

static void paintLayerImage(const TerminalLayer *layer, TerminalCanvas *region) {
    switch ((*layer).fit) {
        case TERMINAL_LAYER_FIT_CONTAIN:
            TerminalImage_renderContain((*layer).image, region);
            break;
        case TERMINAL_LAYER_FIT_NATIVE:
            TerminalImage_render((*layer).image, region, 0, 0);
            break;
        case TERMINAL_LAYER_FIT_STRETCH:
        default:
            TerminalImage_renderFit((*layer).image, region);
            break;
    }
}

TerminalPanel *TerminalPanel_0(void) {
    TerminalPanel *self = Memory_alloc(TYPE_BYTE_ARRAY, sizeof(TerminalPanel));
    if (self == nullptr)
        return nullptr; // R2 owner reports/counts allocation exhaustion.
    *self = (TerminalPanel) {0};
    return self;
}

void TerminalPanel_free(TerminalPanel *self) {
    if (self == nullptr)
        return;
    if ((*self).painting) {
        THROW("TerminalPanel free: mid-paint");
        return;
    }
    if ((*self).layers != nullptr)
        Memory_free((*self).layers);
    *self = (TerminalPanel) {0};
}

TerminalLayerId TerminalPanel_add(TerminalPanel *self, TerminalAnchor anchor) {
    if (self == nullptr || (*self).painting) {
        THROW("TerminalPanel add: null panel or mid-paint");
        return invalidId();
    }
    size_t slot = TERMINAL_PANEL_NO_SLOT;
    for (size_t i = 0; i < (*self).capacity; ++i) {
        TerminalLayer *candidate = &(*self).layers[i];
        if (!(*candidate).used) {
            slot = i;
            break;
        }
    }
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        size_t previous = (*self).capacity;
        if (!growLayers(self))
            return invalidId();
        slot = previous;
    }
    TerminalLayer *layer = &(*self).layers[slot];
    if ((*layer).generation == UINT32_MAX) {
        THROW("TerminalPanel add: layer slot generation exhausted");
        return invalidId();
    }
    uint32_t generation = (*layer).generation + 1;
    memset(layer, 0, sizeof *layer);
    (*layer).used = true;
    (*layer).generation = generation;
    (*layer).visible = true;
    (*layer).anchor = anchor;
    (*layer).pen = TerminalPen_0();
    (*layer).fit = TERMINAL_LAYER_FIT_STRETCH;
    ++(*self).usedCount;
    return (TerminalLayerId) { slot, generation };
}

bool TerminalPanel_remove(TerminalPanel *self, TerminalLayerId id) {
    if (self == nullptr || (*self).painting) {
        THROW("TerminalPanel remove: null panel or mid-paint");
        return false;
    }
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        THROW("TerminalPanel remove: unknown or stale layer id");
        return false;
    }
    TerminalLayer *layer = &(*self).layers[slot];
    (*layer).used = false;
    (*layer).visible = false;
    (*layer).draw = nullptr;
    (*layer).context = nullptr;
    (*layer).image = nullptr;
    if ((*self).usedCount != 0)
        --(*self).usedCount;
    return true;
}

bool TerminalPanel_setAnchor(TerminalPanel *self, TerminalLayerId id, TerminalAnchor anchor) {
    if (self == nullptr) {
        THROW("TerminalPanel anchor: null panel");
        return false;
    }
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        THROW("TerminalPanel anchor: unknown or stale layer id");
        return false;
    }
    TerminalLayer *layer = &(*self).layers[slot];
    (*layer).anchor = anchor;
    return true;
}

TerminalAnchor TerminalPanel_getAnchor(const TerminalPanel *self, TerminalLayerId id) {
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return TerminalAnchor_0();
    const TerminalLayer *layer = &(*self).layers[slot];
    return (*layer).anchor;
}

bool TerminalPanel_setBackgroundColor(TerminalPanel *self, TerminalLayerId id, uint32_t color) {
    if (self == nullptr) {
        THROW("TerminalPanel background: null panel");
        return false;
    }
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        THROW("TerminalPanel background: unknown or stale layer id");
        return false;
    }
    TerminalLayer *layer = &(*self).layers[slot];
    (*layer).pen.bg = color;
    return true;
}

uint32_t TerminalPanel_getBackgroundColor(const TerminalPanel *self, TerminalLayerId id) {
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return TERMINAL_COLOR_DEFAULT;
    const TerminalLayer *layer = &(*self).layers[slot];
    return (*layer).pen.bg;
}

bool TerminalPanel_setForegroundColor(TerminalPanel *self, TerminalLayerId id, uint32_t color) {
    if (self == nullptr) {
        THROW("TerminalPanel foreground: null panel");
        return false;
    }
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        THROW("TerminalPanel foreground: unknown or stale layer id");
        return false;
    }
    TerminalLayer *layer = &(*self).layers[slot];
    (*layer).pen.fg = color;
    return true;
}

uint32_t TerminalPanel_getForegroundColor(const TerminalPanel *self, TerminalLayerId id) {
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return TERMINAL_COLOR_DEFAULT;
    const TerminalLayer *layer = &(*self).layers[slot];
    return (*layer).pen.fg;
}

bool TerminalPanel_setStyle(TerminalPanel *self, TerminalLayerId id, uint8_t style) {
    if (self == nullptr) {
        THROW("TerminalPanel style: null panel");
        return false;
    }
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        THROW("TerminalPanel style: unknown or stale layer id");
        return false;
    }
    TerminalLayer *layer = &(*self).layers[slot];
    (*layer).pen.style = style;
    return true;
}

uint8_t TerminalPanel_getStyle(const TerminalPanel *self, TerminalLayerId id) {
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return TERMINAL_STYLE_NONE;
    const TerminalLayer *layer = &(*self).layers[slot];
    return (*layer).pen.style;
}

bool TerminalPanel_setVisible(TerminalPanel *self, TerminalLayerId id, bool visible) {
    if (self == nullptr) {
        THROW("TerminalPanel visible: null panel");
        return false;
    }
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        THROW("TerminalPanel visible: unknown or stale layer id");
        return false;
    }
    TerminalLayer *layer = &(*self).layers[slot];
    (*layer).visible = visible;
    return true;
}

bool TerminalPanel_isVisible(const TerminalPanel *self, TerminalLayerId id) {
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return false;
    const TerminalLayer *layer = &(*self).layers[slot];
    return (*layer).visible;
}

bool TerminalPanel_setImage(TerminalPanel *self, TerminalLayerId id, const TerminalImage *image,
                            TerminalLayerFit fit) {
    if (self == nullptr) {
        THROW("TerminalPanel image: null panel");
        return false;
    }
    if (fit != TERMINAL_LAYER_FIT_STRETCH && fit != TERMINAL_LAYER_FIT_CONTAIN
        && fit != TERMINAL_LAYER_FIT_NATIVE) {
        THROW("TerminalPanel image: unknown fit");
        return false;
    }
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        THROW("TerminalPanel image: unknown or stale layer id");
        return false;
    }
    TerminalLayer *layer = &(*self).layers[slot];
    (*layer).image = image;
    (*layer).fit = fit;
    return true;
}

const TerminalImage *TerminalPanel_getImage(const TerminalPanel *self, TerminalLayerId id) {
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return nullptr;
    const TerminalLayer *layer = &(*self).layers[slot];
    return (*layer).image;
}

TerminalLayerFit TerminalPanel_getFit(const TerminalPanel *self, TerminalLayerId id) {
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return TERMINAL_LAYER_FIT_STRETCH;
    const TerminalLayer *layer = &(*self).layers[slot];
    return (*layer).fit;
}

bool TerminalPanel_setDraw(TerminalPanel *self, TerminalLayerId id, TerminalLayerDrawFn draw, void *context) {
    if (self == nullptr) {
        THROW("TerminalPanel draw: null panel");
        return false;
    }
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        THROW("TerminalPanel draw: unknown or stale layer id");
        return false;
    }
    TerminalLayer *layer = &(*self).layers[slot];
    (*layer).draw = draw;
    (*layer).context = context;
    return true;
}

TerminalLayerDrawFn TerminalPanel_getDraw(const TerminalPanel *self, TerminalLayerId id) {
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return nullptr;
    const TerminalLayer *layer = &(*self).layers[slot];
    return (*layer).draw;
}

void *TerminalPanel_getContext(const TerminalPanel *self, TerminalLayerId id) {
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return nullptr;
    const TerminalLayer *layer = &(*self).layers[slot];
    return (*layer).context;
}

;;INTENTION("paint is total by design: every visible layer repaints on every call, so the presentation delta stays TerminalSurface's job and this path stays entry-guarded, allocation-free and log-free")
;;HOTCODE
void TerminalPanel_paint(TerminalPanel *self, TerminalCanvas *canvas) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    if ((*self).painting) {
        refuse(self, "reentrant paint");
        return;
    }
    (*self).painting = true;
    TerminalRect cursor = { 0, 0, TerminalCanvas_getWidth(canvas), TerminalCanvas_getHeight(canvas) };
    for (size_t i = 0; i < (*self).capacity; ++i) {
        TerminalLayer *layer = &(*self).layers[i];
        if (!(*layer).used || !(*layer).visible)
            continue;
        TerminalAnchorPlacement placed = TerminalAnchor_place(&(*layer).anchor, cursor);
        cursor = placed.cursor;
        TerminalCanvas region = TerminalCanvas_sub(canvas, placed.region.x, placed.region.y,
                                                  placed.region.width, placed.region.height);
        if (!TerminalCanvas_isValid(&region))
            continue;
        if ((*layer).image != nullptr)
            paintLayerImage(layer, &region);
        TerminalPen pen = (*layer).pen;
        if (TERMINAL_COLOR_IS_SET(pen.bg))
            TerminalCanvas_fill(&region, ' ', pen);
        if ((*layer).draw != nullptr)
            (*layer).draw((*layer).context, &region, placed.region, pen);
    }
    (*self).painting = false;
    ++(*self).paintCount;
}

TerminalRect TerminalPanel_regionOf(const TerminalPanel *self, const TerminalCanvas *canvas, TerminalLayerId id) {
    TerminalRect none = { 0, 0, 0, 0 };
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return none;
    TerminalRect cursor = { 0, 0, TerminalCanvas_getWidth(canvas), TerminalCanvas_getHeight(canvas) };
    for (size_t i = 0; i < (*self).capacity; ++i) {
        const TerminalLayer *layer = &(*self).layers[i];
        if (!(*layer).used || !(*layer).visible)
            continue;
        TerminalAnchorPlacement placed = TerminalAnchor_place(&(*layer).anchor, cursor);
        cursor = placed.cursor;
        if (i == id.index && (*layer).generation == id.generation)
            return placed.region;
    }
    return none;
}

size_t TerminalPanel_layerCount(const TerminalPanel *self) { return self ? (*self).usedCount : 0; }
size_t TerminalPanel_capacity(const TerminalPanel *self) { return self ? (*self).capacity : 0; }
size_t TerminalPanel_paintCount(const TerminalPanel *self) { return self ? (*self).paintCount : 0; }
size_t TerminalPanel_exhaustionCount(const TerminalPanel *self) { return self ? (*self).refusalCount : 0; }
bool TerminalPanel_isPainting(const TerminalPanel *self) { return self != nullptr && (*self).painting; }
bool TerminalPanel_isValid(const TerminalPanel *self, TerminalLayerId id) {
    return slotFor(self, id) != TERMINAL_PANEL_NO_SLOT;
}
bool TerminalPanel_isLayerId(TerminalLayerId id) { return id.generation != 0; }
bool TerminalLayerId_equal(TerminalLayerId a, TerminalLayerId b) {
    return a.index == b.index && a.generation == b.generation;
}
void TerminalPanel_clearExhaustion(TerminalPanel *self) {
    if (self != nullptr)
        (*self).refusalCount = 0;
}

TerminalProjection TerminalPanel_toString(const TerminalPanel *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalPanel{layers=%zu,capacity=%zu,paints=%zu,painting=%d}",
        (*self).usedCount, (*self).capacity, (*self).paintCount, (*self).painting);
}

TerminalProjection TerminalPanel_toStringStruct(const TerminalPanel *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalPanel{layers=<owned>,capacity=%zu,usedCount=%zu,paintCount=%zu,refusalCount=%zu,painting=%d}",
        (*self).capacity, (*self).usedCount, (*self).paintCount, (*self).refusalCount,
        (*self).painting);
}
