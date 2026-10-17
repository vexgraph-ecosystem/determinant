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

// terminal/terminal_panel.h — the LAYERED PANEL.
//
// A TerminalPanel is a stack of layers over one cell region. It owns exactly one
// thing — its growable array of layer records — and nothing about content: a
// layer carries an anchor, a pen, an optional image and an opaque draw callback,
// and the application supplies everything drawn. The toolkit owns geometry and
// z-order; the app owns meaning. That is the same split TerminalFrame already
// makes, with the placement now generalised into TerminalAnchor.
//
// PAINT IS TOTAL. TerminalPanel_paint walks every visible layer, in slot order,
// on every call: no dirty bits, no skipping, no caching. Full repaint is the
// contract because it is predictable and deterministic, and because the cost is
// paid where it belongs — TerminalSurface diffs the freshly painted grid against
// the last paint, so a full repaint still hands the host only the cells that
// changed, and an idle frame hands it nothing at all.
//
// LAYERS COMPOSE BY DEPTH. A layer's draw callback receives the sub-canvas its
// anchor resolved to, so nesting is that callback painting a CHILD panel into
// that region (TerminalPanel_paint(child, &region)). The panel owns no child
// panels and frees none: the application owns its layout tree, the panel owns
// only its own layer array. There is no second mechanism for nesting.
//
// Slot order IS z-order, and it is also flow order: a BAND layer reserves its
// slice so later layers see a smaller cursor, while FILL and PIN layers do not.
// That single rule composes a title band, a status band, two side rails and a
// centre in plain insertion order.

#define TERMINAL_PANEL_LAYERS_DEFAULT 8u

// What a layer does with its borrowed image.
typedef enum TerminalLayerFit {
    TERMINAL_LAYER_FIT_STRETCH = 0,  // box-sample across the whole region (a backdrop)
    TERMINAL_LAYER_FIT_CONTAIN,      // one uniform scale, centred, aspect preserved
    TERMINAL_LAYER_FIT_NATIVE        // 1 source pixel = 1 cell, clipped
} TerminalLayerFit;

// The content painter: draw into the region this layer resolved to, using `pen`
// as the layer's default foreground/background/style. `placement` is that same
// region in the PANEL's coordinates — the canvas carries only its own extent, so
// the placement is how a painter learns where it landed. Borrowed for the call
// only; a callback must not reenter the panel.
typedef void (*TerminalLayerDrawFn)(void *context, TerminalCanvas *region, TerminalRect placement,
                                    TerminalPen pen);

// One layer row. Owned by its panel and never shared; the app reaches it only
// through the panel's id-based operations.
//
// SLOT RECORD: TerminalLayer (no behaviour of its own)
typedef struct TerminalLayer {
    bool used;                 // slot holds a live layer
    uint32_t generation;       // bumped on slot reuse, so a stale id is detectable
    bool visible;              // hidden layers neither draw nor reserve
    TerminalAnchor anchor;     // placement in the parent
    TerminalPen pen;           // layer default style/foreground/background
    const TerminalImage *image; // borrowed backdrop/overlay (nullable)
    TerminalLayerFit fit;      // how the image fills its region
    TerminalLayerDrawFn draw;  // borrowed content painter (nullable)
    void *context;             // opaque painter context
} TerminalLayer;

// A layer handle: slot plus the generation that slot held when the id was
// issued. Zero is invalid. A removed-and-reused slot reports a stale id instead
// of silently addressing a different layer.
//
// SLOT RECORD: TerminalLayerId (no behaviour of its own)
typedef struct TerminalLayerId {
    size_t index;        // slot index
    uint32_t generation; // generation expected at that slot (0 = invalid)
} TerminalLayerId;

typedef struct TerminalPanel {
    TerminalLayer *layers;   // owned growable slot array
    size_t capacity;         // admitted slots
    size_t usedCount;        // live layers
    size_t paintCount;       // completed paints
    size_t refusalCount;     // refusals in this epoch
    bool painting;           // a paint is in flight (reentrancy guard)
} TerminalPanel;

TerminalPanel *TerminalPanel_0(void);
#define TerminalPanel(...) CONSTRUCTOR_DISPATCH(TerminalPanel, __VA_ARGS__)
#define TerminalPanel_zero() TerminalPanel_0()

// Releases the layer array. Null is harmless; a panel mid-paint is refused.
void TerminalPanel_free(TerminalPanel *self);

// Append a layer (cold; grows the slot array). Returns the new id, or a zero id
// when the array could not grow, in which case every existing layer is intact.
TerminalLayerId TerminalPanel_add(TerminalPanel *self, TerminalAnchor anchor);
// Retire a layer; its slot is reused by a later add and its id then goes stale.
bool TerminalPanel_remove(TerminalPanel *self, TerminalLayerId id);

// Cold configuration. Every operation validates the id and rejects loudly
// without mutating anything when it is unknown, stale or already retired.
bool TerminalPanel_setAnchor(TerminalPanel *self, TerminalLayerId id, TerminalAnchor anchor);
TerminalAnchor TerminalPanel_getAnchor(const TerminalPanel *self, TerminalLayerId id);
bool TerminalPanel_setBackgroundColor(TerminalPanel *self, TerminalLayerId id, uint32_t color);
uint32_t TerminalPanel_getBackgroundColor(const TerminalPanel *self, TerminalLayerId id);
bool TerminalPanel_setForegroundColor(TerminalPanel *self, TerminalLayerId id, uint32_t color);
uint32_t TerminalPanel_getForegroundColor(const TerminalPanel *self, TerminalLayerId id);
bool TerminalPanel_setStyle(TerminalPanel *self, TerminalLayerId id, uint8_t style);
uint8_t TerminalPanel_getStyle(const TerminalPanel *self, TerminalLayerId id);
bool TerminalPanel_setVisible(TerminalPanel *self, TerminalLayerId id, bool visible);
bool TerminalPanel_isVisible(const TerminalPanel *self, TerminalLayerId id);
bool TerminalPanel_setImage(TerminalPanel *self, TerminalLayerId id, const TerminalImage *image,
                            TerminalLayerFit fit);
const TerminalImage *TerminalPanel_getImage(const TerminalPanel *self, TerminalLayerId id);
TerminalLayerFit TerminalPanel_getFit(const TerminalPanel *self, TerminalLayerId id);
bool TerminalPanel_setDraw(TerminalPanel *self, TerminalLayerId id, TerminalLayerDrawFn draw, void *context);
TerminalLayerDrawFn TerminalPanel_getDraw(const TerminalPanel *self, TerminalLayerId id);
void *TerminalPanel_getContext(const TerminalPanel *self, TerminalLayerId id);

// Hot: paint EVERY visible layer, in slot order, every call. Quiet on a clean
// paint — no allocation, no logging, no per-cell validation. A reentrant paint is
// refused and counted (the Exhaustion Loudness Law), reporting once per epoch.
void TerminalPanel_paint(TerminalPanel *self, TerminalCanvas *canvas);

// Cold query: the region a layer resolved to, walking the reserve flow from the
// first layer. A zero rect for an unknown, stale or hidden id. This is the
// hit-test primitive a pointer event uses, so it belongs on the cold path.
TerminalRect TerminalPanel_regionOf(const TerminalPanel *self, const TerminalCanvas *canvas, TerminalLayerId id);

size_t TerminalPanel_layerCount(const TerminalPanel *self);
size_t TerminalPanel_capacity(const TerminalPanel *self);
size_t TerminalPanel_paintCount(const TerminalPanel *self);
size_t TerminalPanel_exhaustionCount(const TerminalPanel *self);
bool TerminalPanel_isPainting(const TerminalPanel *self);
bool TerminalPanel_isValid(const TerminalPanel *self, TerminalLayerId id);
bool TerminalPanel_isLayerId(TerminalLayerId id);
bool TerminalLayerId_equal(TerminalLayerId a, TerminalLayerId b);
// Start a fresh refusal epoch (the Exhaustion Loudness Law's reset).
void TerminalPanel_clearExhaustion(TerminalPanel *self);

TerminalProjection TerminalPanel_toString(const TerminalPanel *self);
TerminalProjection TerminalPanel_toStringStruct(const TerminalPanel *self);

#endif
