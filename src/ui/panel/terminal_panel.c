#include "ui/panel/terminal_panel.h"
#include "ui/text/terminal_text.h"
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
/** The container. A panel holds children and arranges them, and that is all it
 * does: it has a pen, an optional background image, a layout flavour and a slot
 * array. Its children are panels and text leaves, and either may be a child of
 * either, so the interface is one tree and not a pile of special cases.
 *
 * Two flavours, one line apart. FLEX advances a cursor past each child that
 * reserves its slice, so a title band, a rail and a centre compose in insertion
 * order; LAYERED never advances it, so every child resolves against the whole
 * area and z-order is simply the order they were added. The anchor still decides
 * WHERE — a band child in a layered panel keeps its own slice of the full area,
 * which is what lets an overlay sit at a pin above a flowing layout. Neither
 * flavour owns any geometry of its own — TerminalAnchor answers where each child
 * lands.
 *
 * Resize is why the tree exists at all. Every rectangle is recomputed from the
 * canvas it is handed on every paint, so nothing stores a size and a resize is
 * just the next paint at new dimensions. A panel that cached a rectangle would
 * need invalidation, dirty flags and a second code path; this one needs none.
 *
 * Ownership transfers on add and returns on remove or free, recursively. The
 * panel never reaches into a child's state to draw it — it hands the child the
 * region it resolved and the pen it wants inherited, and the child decides. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalPanel (ui/panel/terminal_panel.c)
 * STRUCT FIELDS (Mirroring ui/panel/terminal_panel.h)
 * TerminalPanelLayout layout    FLEX (bands reserve) or LAYERED (stack)
 * TerminalChild *children       owned growable slot array
 * size_t capacity               admitted slots
 * size_t usedCount              live children
 * size_t paintCount             completed paints
 * size_t refusalCount           refusals in this epoch
 * TerminalPen pen               inherited by the children added directly here
 * const TerminalImage *image    borrowed background (nullable)
 * TerminalPanelImageFit fit     how the background fills the area
 * bool painting                 a paint is in flight (reentrancy guard)
 * SLOT RECORD: TerminalChild
 * bool used                     slot holds a live child
 * uint32_t generation           bumped on slot reuse, so a stale id is detectable
 * TerminalComponentKind kind    which payload is live
 * TerminalAnchor anchor         placement in the parent
 * TerminalPanel *panel          owned nested panel when kind == PANEL
 * TerminalText *text            owned text leaf when kind == TEXT
 * SLOT RECORD: TerminalChildId
 * size_t index                  slot index
 * uint32_t generation           the generation expected at that slot (0 = invalid)
 * PRIVATE HELPERS
 * TERMINAL_PANEL_NO_SLOT      sentinel index meaning "no live child here"
 * invalidId()                 the zero id every refusal returns
 * slotFor(self, id)           validate id -> slot, or NO_SLOT when unknown/stale
 * refuse(self, reason)        count a refusal, reporting once per epoch
 * growChildren(self)          cold doubling growth preserving every live child
 * freeChild(child)            release one child payload (recursively for panels)
 * paintImage(self, canvas)    borrowed background through its declared fit
 * paintChildren(self, canvas) place, then paint every child in slot order
 * FUNCTION REGISTRY (exports)
 * Constructor: TerminalPanel *TerminalPanel_0(void) [zero/arity chooser]
 * Lifecycle: void TerminalPanel_free(self)
 * Layout: bool setLayout/getLayout(self)
 * Children: TerminalChildId addPanel/addText(self, child, anchor)
 *   bool remove(self, id)
 *   TerminalRect childRegion(self, canvas, id)
 *   TerminalChildId childAt(self, canvas, x, y)
 *   TerminalComponentKind childKind(self, id)
 *   TerminalPanel *childPanel(self, id), TerminalText *childText(self, id)
 * Configuration: bool setPen/getPen, setStyle/getStyle,
 *   setForegroundColor/getForegroundColor, setBackgroundColor/getBackgroundColor,
 *   setImage/getImage/getFit, setAnchor/getAnchor
 * Paint: void TerminalPanel_paint(self, canvas)
 * Queries: size_t childCount/capacity/paintCount/exhaustionCount(self)
 *   bool isValid(self, id), isPainting(self), isChildId(id)
 *   bool TerminalChildId_equal(a, b), void clearExhaustion(self)
 * Projections: TerminalProjection TerminalPanel_toString/toStringStruct(self)
 * CONTRACT: cold operations validate the id and reject loudly without mutating
 * anything — an unknown, stale or retired id touches no child. Paint is hot and
 * total: one entry guard, no allocation, no logging, no per-cell validation, and
 * a reentrant paint is refused, counted and reported at most once per epoch.
 * Adding transfers ownership; a refused add leaves the child with the caller.
 * Freeing a panel frees its whole subtree. Template/type identity is not claimed
 * here: a panel is a panel, and its flavour is a field.
 * ======================================================================== */

#define TERMINAL_PANEL_NO_SLOT ((size_t) -1)

static TerminalChildId invalidId(void) { return (TerminalChildId) { 0, 0 }; }

static void refuse(TerminalPanel *self, const char *reason) {
    if ((*self).refusalCount == 0)
        THROW("TerminalPanel exhausted: %s (capacity %zu, live %zu)",
              reason, (*self).capacity, (*self).usedCount);
    if ((*self).refusalCount != SIZE_MAX)
        ++(*self).refusalCount;
}

// Resolve an id to a live slot. Zero generation, out-of-range index, a retired
// slot and a generation mismatch all mean "no such child" — never a silent
// substitute for a recycled one.
static size_t slotFor(const TerminalPanel *self, TerminalChildId id) {
    if (self == nullptr || id.generation == 0 || id.index >= (*self).capacity)
        return TERMINAL_PANEL_NO_SLOT;
    const TerminalChild *child = &(*self).children[id.index];
    if (!(*child).used || (*child).generation != id.generation)
        return TERMINAL_PANEL_NO_SLOT;
    return id.index;
}

// Cold doubling growth. Every live child survives; a failed allocation leaves the
// panel exactly as it was and is reported through the refusal channel.
static bool growChildren(TerminalPanel *self) {
    size_t next = (*self).capacity == 0 ? TERMINAL_PANEL_CHILDREN_DEFAULT : (*self).capacity * 2;
    if (next < (*self).capacity || next > SIZE_MAX / sizeof(TerminalChild)) {
        refuse(self, "child capacity overflow");
        return false;
    }
    TerminalChild *grown = Memory_alloc(TYPE_BYTE_ARRAY, next * sizeof(TerminalChild));
    if (grown == nullptr) {
        refuse(self, "child array allocation refused");
        return false;
    }
    if ((*self).capacity != 0)
        memcpy(grown, (*self).children, (*self).capacity * sizeof(TerminalChild));
    memset(grown + (*self).capacity, 0, (next - (*self).capacity) * sizeof(TerminalChild));
    Memory_free((*self).children);
    (*self).children = grown;
    (*self).capacity = next;
    return true;
}

// Release one child's payload. A nested panel takes its own subtree with it.
static void freeChild(TerminalChild *child) {
    if (child == nullptr)
        return;
    if ((*child).kind == TERMINAL_COMPONENT_PANEL)
        TerminalPanel_free((*child).panel);
    else
        TerminalText_free((*child).text);
    (*child).panel = nullptr;
    (*child).text = nullptr;
    (*child).used = false;
}

static void paintImage(const TerminalPanel *self, TerminalCanvas *canvas) {
    switch ((*self).fit) {
        case TERMINAL_PANEL_IMAGE_CONTAIN:
            TerminalImage_renderContain((*self).image, canvas);
            break;
        case TERMINAL_PANEL_IMAGE_NATIVE:
            TerminalImage_render((*self).image, canvas, 0, 0);
            break;
        case TERMINAL_PANEL_IMAGE_STRETCH:
        default:
            TerminalImage_renderFit((*self).image, canvas);
            break;
    }
}

static void paintChildren(TerminalPanel *self, TerminalCanvas *canvas) {
    if ((*self).image != nullptr)
        paintImage(self, canvas);
    TerminalPen pen = (*self).pen;
    if (TERMINAL_COLOR_IS_SET(pen.bg))
        TerminalCanvas_fill(canvas, ' ', pen);
    bool flowing = (*self).layout == TERMINAL_PANEL_FLEX;
    TerminalRect cursor = { 0, 0, TerminalCanvas_getWidth(canvas), TerminalCanvas_getHeight(canvas) };
    for (size_t i = 0; i < (*self).capacity; ++i) {
        TerminalChild *child = &(*self).children[i];
        if (!(*child).used)
            continue;
        TerminalAnchorPlacement placed = TerminalAnchor_place(&(*child).anchor, cursor);
        if (flowing)
            cursor = placed.cursor;
        TerminalCanvas region = TerminalCanvas_sub(canvas, placed.region.x, placed.region.y,
                                                  placed.region.width, placed.region.height);
        if (!TerminalCanvas_isValid(&region))
            continue;
        if ((*child).kind == TERMINAL_COMPONENT_PANEL)
            TerminalPanel_paint((*child).panel, &region);
        else
            TerminalText_paint((*child).text, &region, pen);
    }
}

TerminalPanel *TerminalPanel_0(void) {
    TerminalPanel *self = Memory_alloc(TYPE_BYTE_ARRAY, sizeof(TerminalPanel));
    if (self == nullptr)
        return nullptr; // R2 owner reports/counts allocation exhaustion.
    *self = (TerminalPanel) { .layout = TERMINAL_PANEL_FLEX, .pen = TerminalPen_0() };
    return self;
}

void TerminalPanel_free(TerminalPanel *self) {
    if (self == nullptr)
        return;
    if ((*self).painting) {
        THROW("TerminalPanel free: mid-paint");
        return;
    }
    for (size_t i = 0; i < (*self).capacity; ++i) {
        TerminalChild *child = &(*self).children[i];
        if ((*child).used)
            freeChild(child);
    }
    if ((*self).children != nullptr)
        Memory_free((*self).children);
    *self = (TerminalPanel) {0};
}

bool TerminalPanel_setLayout(TerminalPanel *self, TerminalPanelLayout layout) {
    if (self == nullptr
        || (layout != TERMINAL_PANEL_FLEX && layout != TERMINAL_PANEL_LAYERED)) {
        THROW("TerminalPanel layout: null panel or unknown layout");
        return false;
    }
    (*self).layout = layout;
    return true;
}

TerminalPanelLayout TerminalPanel_getLayout(const TerminalPanel *self) {
    return self ? (*self).layout : TERMINAL_PANEL_FLEX;
}

// Shared tail of both adds: take a free slot, take ownership, publish the id.
static TerminalChildId adoptChild(TerminalPanel *self, TerminalComponentKind kind,
                                 void *payload, TerminalAnchor anchor) {
    if (self == nullptr || payload == nullptr || (*self).painting) {
        THROW("TerminalPanel add: null panel, null child or mid-paint");
        return invalidId();
    }
    size_t slot = TERMINAL_PANEL_NO_SLOT;
    for (size_t i = 0; i < (*self).capacity; ++i) {
        TerminalChild *candidate = &(*self).children[i];
        if (!(*candidate).used) {
            slot = i;
            break;
        }
    }
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        size_t previous = (*self).capacity;
        if (!growChildren(self))
            return invalidId();
        slot = previous;
    }
    TerminalChild *child = &(*self).children[slot];
    if ((*child).generation == UINT32_MAX) {
        THROW("TerminalPanel add: child slot generation exhausted");
        return invalidId();
    }
    uint32_t generation = (*child).generation + 1;
    memset(child, 0, sizeof *child);
    (*child).generation = generation;
    (*child).used = true;
    (*child).kind = kind;
    (*child).anchor = anchor;
    if (kind == TERMINAL_COMPONENT_PANEL)
        (*child).panel = payload;
    else
        (*child).text = payload;
    ++(*self).usedCount;
    return (TerminalChildId) { slot, generation };
}

TerminalChildId TerminalPanel_addPanel(TerminalPanel *self, TerminalPanel *child, TerminalAnchor anchor) {
    if (child == nullptr) {
        THROW("TerminalPanel addPanel: null child");
        return invalidId();
    }
    if (child == self) {
        // A panel cannot be its own child: the tree would never terminate.
        THROW("TerminalPanel addPanel: a panel cannot contain itself");
        return invalidId();
    }
    return adoptChild(self, TERMINAL_COMPONENT_PANEL, child, anchor);
}

TerminalChildId TerminalPanel_addText(TerminalPanel *self, TerminalText *child, TerminalAnchor anchor) {
    if (child == nullptr) {
        THROW("TerminalPanel addText: null child");
        return invalidId();
    }
    return adoptChild(self, TERMINAL_COMPONENT_TEXT, child, anchor);
}

bool TerminalPanel_remove(TerminalPanel *self, TerminalChildId id) {
    if (self == nullptr || (*self).painting) {
        THROW("TerminalPanel remove: null panel or mid-paint");
        return false;
    }
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        THROW("TerminalPanel remove: unknown or stale child id");
        return false;
    }
    TerminalChild *child = &(*self).children[slot];
    freeChild(child);
    if ((*self).usedCount != 0)
        --(*self).usedCount;
    return true;
}

// Detach one child without freeing it. The slot is retired so the old id goes
// stale, and ownership returns to the caller for re-parenting.
static void *detachChild(TerminalPanel *self, TerminalChildId id, TerminalComponentKind kind) {
    if (self == nullptr || (*self).painting)
        return nullptr;
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return nullptr;
    TerminalChild *child = &(*self).children[slot];
    if ((*child).kind != kind)
        return nullptr;
    void *payload = kind == TERMINAL_COMPONENT_PANEL ? (void*) (*child).panel : (void*) (*child).text;
    (*child).panel = nullptr;
    (*child).text = nullptr;
    (*child).used = false;
    if ((*self).usedCount != 0)
        --(*self).usedCount;
    return payload;
}

TerminalPanel *TerminalPanel_detachPanel(TerminalPanel *self, TerminalChildId id) {
    return detachChild(self, id, TERMINAL_COMPONENT_PANEL);
}

TerminalText *TerminalPanel_detachText(TerminalPanel *self, TerminalChildId id) {
    return detachChild(self, id, TERMINAL_COMPONENT_TEXT);
}

bool TerminalPanel_setPen(TerminalPanel *self, TerminalPen pen) {
    if (self == nullptr) {
        THROW("TerminalPanel pen: null panel");
        return false;
    }
    (*self).pen = pen;
    return true;
}
TerminalPen TerminalPanel_getPen(const TerminalPanel *self) {
    return self ? (*self).pen : TerminalPen_0();
}
bool TerminalPanel_setStyle(TerminalPanel *self, uint8_t style) {
    if (self == nullptr) {
        THROW("TerminalPanel style: null panel");
        return false;
    }
    TerminalPen *pen = &(*self).pen;
    (*pen).style = style;
    return true;
}
uint8_t TerminalPanel_getStyle(const TerminalPanel *self) {
    if (self == nullptr)
        return TERMINAL_STYLE_NONE;
    const TerminalPen *pen = &(*self).pen;
    return (*pen).style;
}
bool TerminalPanel_setForegroundColor(TerminalPanel *self, uint32_t color) {
    if (self == nullptr) {
        THROW("TerminalPanel foreground: null panel");
        return false;
    }
    TerminalPen *pen = &(*self).pen;
    (*pen).fg = color;
    return true;
}
uint32_t TerminalPanel_getForegroundColor(const TerminalPanel *self) {
    if (self == nullptr)
        return TERMINAL_COLOR_DEFAULT;
    const TerminalPen *pen = &(*self).pen;
    return (*pen).fg;
}
bool TerminalPanel_setBackgroundColor(TerminalPanel *self, uint32_t color) {
    if (self == nullptr) {
        THROW("TerminalPanel background: null panel");
        return false;
    }
    TerminalPen *pen = &(*self).pen;
    (*pen).bg = color;
    return true;
}
uint32_t TerminalPanel_getBackgroundColor(const TerminalPanel *self) {
    if (self == nullptr)
        return TERMINAL_COLOR_DEFAULT;
    const TerminalPen *pen = &(*self).pen;
    return (*pen).bg;
}

bool TerminalPanel_setImage(TerminalPanel *self, const TerminalImage *image, TerminalPanelImageFit fit) {
    if (self == nullptr) {
        THROW("TerminalPanel image: null panel");
        return false;
    }
    if (fit != TERMINAL_PANEL_IMAGE_STRETCH && fit != TERMINAL_PANEL_IMAGE_CONTAIN
        && fit != TERMINAL_PANEL_IMAGE_NATIVE) {
        THROW("TerminalPanel image: unknown fit");
        return false;
    }
    (*self).image = image;
    (*self).fit = fit;
    return true;
}
const TerminalImage *TerminalPanel_getImage(const TerminalPanel *self) { return self ? (*self).image : nullptr; }
TerminalPanelImageFit TerminalPanel_getFit(const TerminalPanel *self) {
    return self ? (*self).fit : TERMINAL_PANEL_IMAGE_STRETCH;
}

bool TerminalPanel_setAnchor(TerminalPanel *self, TerminalChildId id, TerminalAnchor anchor) {
    if (self == nullptr) {
        THROW("TerminalPanel anchor: null panel");
        return false;
    }
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT) {
        THROW("TerminalPanel anchor: unknown or stale child id");
        return false;
    }
    TerminalChild *child = &(*self).children[slot];
    (*child).anchor = anchor;
    return true;
}
TerminalAnchor TerminalPanel_getAnchor(const TerminalPanel *self, TerminalChildId id) {
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return TerminalAnchor_0();
    const TerminalChild *child = &(*self).children[slot];
    return (*child).anchor;
}

;;INTENTION("paint is total by design: every child repaints on every call, so a resize needs no invalidation and the presentation delta stays TerminalSurface's job; this path stays entry-guarded, allocation-free and log-free")
;;HOTCODE
void TerminalPanel_paint(TerminalPanel *self, TerminalCanvas *canvas) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    if ((*self).painting) {
        refuse(self, "reentrant paint");
        return;
    }
    (*self).painting = true;
    paintChildren(self, canvas);
    (*self).painting = false;
    ++(*self).paintCount;
}

TerminalRect TerminalPanel_childRegion(const TerminalPanel *self, const TerminalCanvas *canvas,
                                       TerminalChildId id) {
    TerminalRect none = { 0, 0, 0, 0 };
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return none;
    bool flowing = (*self).layout == TERMINAL_PANEL_FLEX;
    TerminalRect cursor = { 0, 0, TerminalCanvas_getWidth(canvas), TerminalCanvas_getHeight(canvas) };
    for (size_t i = 0; i < (*self).capacity; ++i) {
        const TerminalChild *child = &(*self).children[i];
        if (!(*child).used)
            continue;
        TerminalAnchorPlacement placed = TerminalAnchor_place(&(*child).anchor, cursor);
        if (flowing)
            cursor = placed.cursor;
        if (i == id.index && (*child).generation == id.generation)
            return placed.region;
    }
    return none;
}

TerminalChildId TerminalPanel_childAt(const TerminalPanel *self, const TerminalCanvas *canvas,
                                      size_t x, size_t y) {
    TerminalChildId hit = invalidId();
    if (self == nullptr || canvas == nullptr || !TerminalCanvas_isValid(canvas))
        return hit;
    bool flowing = (*self).layout == TERMINAL_PANEL_FLEX;
    TerminalRect cursor = { 0, 0, TerminalCanvas_getWidth(canvas), TerminalCanvas_getHeight(canvas) };
    for (size_t i = 0; i < (*self).capacity; ++i) {
        const TerminalChild *child = &(*self).children[i];
        if (!(*child).used)
            continue;
        TerminalAnchorPlacement placed = TerminalAnchor_place(&(*child).anchor, cursor);
        if (flowing)
            cursor = placed.cursor;
        TerminalRect region = placed.region;
        if (region.width == 0 || region.height == 0)
            continue;
        if (x < region.x || x >= region.x + region.width || y < region.y || y >= region.y + region.height)
            continue;
        hit = (TerminalChildId) { i, (*child).generation };   // later children are on top
    }
    return hit;
}

TerminalComponentKind TerminalPanel_childKind(const TerminalPanel *self, TerminalChildId id) {
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return TERMINAL_COMPONENT_TEXT;
    const TerminalChild *child = &(*self).children[slot];
    return (*child).kind;
}

TerminalPanel *TerminalPanel_childPanel(const TerminalPanel *self, TerminalChildId id) {
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return nullptr;
    const TerminalChild *child = &(*self).children[slot];
    return (*child).kind == TERMINAL_COMPONENT_PANEL ? (*child).panel : nullptr;
}

TerminalText *TerminalPanel_childText(const TerminalPanel *self, TerminalChildId id) {
    size_t slot = slotFor(self, id);
    if (slot == TERMINAL_PANEL_NO_SLOT)
        return nullptr;
    const TerminalChild *child = &(*self).children[slot];
    return (*child).kind == TERMINAL_COMPONENT_TEXT ? (*child).text : nullptr;
}

size_t TerminalPanel_childCount(const TerminalPanel *self) { return self ? (*self).usedCount : 0; }
size_t TerminalPanel_capacity(const TerminalPanel *self) { return self ? (*self).capacity : 0; }
size_t TerminalPanel_paintCount(const TerminalPanel *self) { return self ? (*self).paintCount : 0; }
size_t TerminalPanel_exhaustionCount(const TerminalPanel *self) { return self ? (*self).refusalCount : 0; }
bool TerminalPanel_isPainting(const TerminalPanel *self) { return self != nullptr && (*self).painting; }
bool TerminalPanel_isValid(const TerminalPanel *self, TerminalChildId id) {
    return slotFor(self, id) != TERMINAL_PANEL_NO_SLOT;
}
bool TerminalPanel_isChildId(TerminalChildId id) { return id.generation != 0; }
bool TerminalChildId_equal(TerminalChildId a, TerminalChildId b) {
    return a.index == b.index && a.generation == b.generation;
}
void TerminalPanel_clearExhaustion(TerminalPanel *self) {
    if (self != nullptr)
        (*self).refusalCount = 0;
}

TerminalProjection TerminalPanel_toString(const TerminalPanel *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format("TerminalPanel{layout=%d,children=%zu,capacity=%zu,paints=%zu}",
        (*self).layout, (*self).usedCount, (*self).capacity, (*self).paintCount);
}

TerminalProjection TerminalPanel_toStringStruct(const TerminalPanel *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    const TerminalPen *pen = &(*self).pen;
    return TerminalProjection_format(
        "TerminalPanel{layout=%d,children=<owned>,capacity=%zu,usedCount=%zu,paintCount=%zu,refusalCount=%zu,pen={style=0x%02X,fg=0x%08X,bg=0x%08X},image=%s,fit=%d,painting=%d}",
        (*self).layout, (*self).capacity, (*self).usedCount, (*self).paintCount,
        (*self).refusalCount, (*pen).style, (*pen).fg, (*pen).bg,
        (*self).image ? "bound" : "none", (*self).fit, (*self).painting);
}
