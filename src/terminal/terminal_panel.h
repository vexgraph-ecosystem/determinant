#ifndef DETERMINANT_TERMINAL_PANEL_H
#define DETERMINANT_TERMINAL_PANEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "terminal/terminal_anchor.h"
#include "terminal/terminal_canvas.h"
#include "terminal/terminal_image.h"
#include "terminal/terminal_projection.h"
#include "c23/constructor.h"

// terminal/terminal_panel.h — the CONTAINER.
//
// One of the two components a terminal interface is made of. A panel is a box
// that holds children and arranges them; it has no content of its own beyond a
// pen and an optional background image. A child is either another panel or a text
// leaf, and either can be a child of either — that is the whole tree.
//
// The flavour of a panel is ONE field, not one class per kind:
//   FLEX      children flow: a band anchor takes its slice and reserves it, so
//             the next child sees a smaller cursor;
//   LAYERED   children stack: the cursor never shrinks, so every child resolves
//             against the whole area — a FILL child covers it, a BAND child keeps
//             its own slice OF it, and z-order is just the order you added them.
// The difference is a single line — whether the cursor advances — because the
// reserve rule already lives in TerminalAnchor.
//
// RESIZE IS FREE. Every rectangle is recomputed from the canvas size it is given
// on every paint, top-down through the tree. Nothing caches a size, so a resize
// is simply the next paint at new dimensions: the host hands the root a canvas of
// the new size and the tree follows. That is why a component tree is worth having.
//
// OWNERSHIP: adding a child transfers it to the panel. Removing a child (or
// freeing the panel) frees it, recursively for nested panels. A refused add
// leaves the child unowned — the caller still owns it and may retry or free it.
// Nothing is borrowed except glyph strings and images, which the caller must keep
// alive.
//
// The panel's pen is inherited by the children ADDED DIRECTLY TO IT, so a panel
// can say "everything on me is grey" once. A nested panel has its own pen.

#define TERMINAL_PANEL_CHILDREN_DEFAULT 4u

typedef struct TerminalPanel TerminalPanel;
typedef struct TerminalText TerminalText;

typedef enum TerminalPanelLayout {
    TERMINAL_PANEL_FLEX = 0,   // flow: bands reserve their slice
    TERMINAL_PANEL_LAYERED     // stack: every child gets the whole area
} TerminalPanelLayout;

typedef enum TerminalComponentKind {
    TERMINAL_COMPONENT_PANEL = 0,
    TERMINAL_COMPONENT_TEXT
} TerminalComponentKind;

// How a panel's borrowed background image fills its area.
typedef enum TerminalPanelImageFit {
    TERMINAL_PANEL_IMAGE_STRETCH = 0,  // box-sample across the whole area
    TERMINAL_PANEL_IMAGE_CONTAIN,      // one uniform scale, centred
    TERMINAL_PANEL_IMAGE_NATIVE        // 1 source pixel = 1 cell, clipped
} TerminalPanelImageFit;

// One child slot. Owned by its panel and never shared; exactly one payload is
// live, chosen by `kind`.
//
// SLOT RECORD: TerminalChild (no behaviour of its own)
typedef struct TerminalChild {
    bool used;                  // slot holds a live child
    uint32_t generation;        // bumped on slot reuse, so a stale id is detectable
    TerminalComponentKind kind; // which payload is live
    TerminalAnchor anchor;      // placement in the parent
    TerminalPanel *panel;       // owned nested panel when kind == PANEL
    TerminalText *text;         // owned text leaf when kind == TEXT
} TerminalChild;

// A child handle: slot plus the generation that slot held. Zero is invalid, and a
// removed-and-reused slot reports a stale id rather than addressing a new child.
//
// SLOT RECORD: TerminalChildId (no behaviour of its own)
typedef struct TerminalChildId {
    size_t index;        // slot index
    uint32_t generation; // generation expected at that slot (0 = invalid)
} TerminalChildId;

struct TerminalPanel {
    TerminalPanelLayout layout;  // FLEX or LAYERED
    TerminalChild *children;     // owned growable slot array
    size_t capacity;             // admitted slots
    size_t usedCount;            // live children
    size_t paintCount;           // completed paints
    size_t refusalCount;         // refusals in this epoch
    TerminalPen pen;             // inherited by the children added directly here
    const TerminalImage *image;  // borrowed background (nullable)
    TerminalPanelImageFit fit;   // how the background fills the area
    bool painting;               // a paint is in flight (reentrancy guard)
};

TerminalPanel *TerminalPanel_0(void);
#define TerminalPanel(...) CONSTRUCTOR_DISPATCH(TerminalPanel, __VA_ARGS__)
#define TerminalPanel_zero() TerminalPanel_0()

// Frees the panel, every child, and every nested panel beneath them. Null is
// harmless; a panel mid-paint is refused.
void TerminalPanel_free(TerminalPanel *self);

bool TerminalPanel_setLayout(TerminalPanel *self, TerminalPanelLayout layout);
TerminalPanelLayout TerminalPanel_getLayout(const TerminalPanel *self);

// Add a child, transferring ownership to this panel. A zero id means the add was
// refused (an allocation failed, the panel is mid-paint, or the child is null),
// and the caller still owns the child.
TerminalChildId TerminalPanel_addPanel(TerminalPanel *self, TerminalPanel *child, TerminalAnchor anchor);
TerminalChildId TerminalPanel_addText(TerminalPanel *self, TerminalText *child, TerminalAnchor anchor);
// Free one child and retire its slot; the id then goes stale.
bool TerminalPanel_remove(TerminalPanel *self, TerminalChildId id);
// Take a child back WITHOUT freeing it, so it can be re-parented. Returns null
// when the id is unknown or stale, or when the child is of the other kind.
TerminalPanel *TerminalPanel_detachPanel(TerminalPanel *self, TerminalChildId id);
TerminalText *TerminalPanel_detachText(TerminalPanel *self, TerminalChildId id);

bool TerminalPanel_setPen(TerminalPanel *self, TerminalPen pen);
TerminalPen TerminalPanel_getPen(const TerminalPanel *self);
bool TerminalPanel_setStyle(TerminalPanel *self, uint8_t style);
uint8_t TerminalPanel_getStyle(const TerminalPanel *self);
bool TerminalPanel_setForegroundColor(TerminalPanel *self, uint32_t color);
uint32_t TerminalPanel_getForegroundColor(const TerminalPanel *self);
bool TerminalPanel_setBackgroundColor(TerminalPanel *self, uint32_t color);
uint32_t TerminalPanel_getBackgroundColor(const TerminalPanel *self);
bool TerminalPanel_setImage(TerminalPanel *self, const TerminalImage *image, TerminalPanelImageFit fit);
const TerminalImage *TerminalPanel_getImage(const TerminalPanel *self);
TerminalPanelImageFit TerminalPanel_getFit(const TerminalPanel *self);

bool TerminalPanel_setAnchor(TerminalPanel *self, TerminalChildId id, TerminalAnchor anchor);
TerminalAnchor TerminalPanel_getAnchor(const TerminalPanel *self, TerminalChildId id);

// Hot: paint the background, then EVERY child, in slot order, every call. Quiet
// on a clean paint — no allocation, no logging, no per-cell validation. A
// reentrant paint is refused and counted.
void TerminalPanel_paint(TerminalPanel *self, TerminalCanvas *canvas);

// Cold: the rectangle a child resolved to, and the top-most child containing a
// cell. These are the hit-test primitives a pointer event uses, so they live on
// the cold path. A zero rect or zero id means "no such child here".
TerminalRect TerminalPanel_childRegion(const TerminalPanel *self, const TerminalCanvas *canvas, TerminalChildId id);
TerminalChildId TerminalPanel_childAt(const TerminalPanel *self, const TerminalCanvas *canvas, size_t x, size_t y);
TerminalComponentKind TerminalPanel_childKind(const TerminalPanel *self, TerminalChildId id);
TerminalPanel *TerminalPanel_childPanel(const TerminalPanel *self, TerminalChildId id);
TerminalText *TerminalPanel_childText(const TerminalPanel *self, TerminalChildId id);

size_t TerminalPanel_childCount(const TerminalPanel *self);
size_t TerminalPanel_capacity(const TerminalPanel *self);
size_t TerminalPanel_paintCount(const TerminalPanel *self);
size_t TerminalPanel_exhaustionCount(const TerminalPanel *self);
bool TerminalPanel_isPainting(const TerminalPanel *self);
bool TerminalPanel_isValid(const TerminalPanel *self, TerminalChildId id);
bool TerminalPanel_isChildId(TerminalChildId id);
bool TerminalChildId_equal(TerminalChildId a, TerminalChildId b);
void TerminalPanel_clearExhaustion(TerminalPanel *self);

TerminalProjection TerminalPanel_toString(const TerminalPanel *self);
TerminalProjection TerminalPanel_toStringStruct(const TerminalPanel *self);

#endif
