#include "terminal/terminal_list_view.h"
#include "terminal/terminal_style.h"
#include "c23/constructor.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** A generic list: borrowed labels drawn one per row with a scroll offset and a
 * single selection marker. Meaningless on its own — channels, users, commands
 * and settings are all the same widget. Owns no memory; the caller supplies the
 * labels and the two pens. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalListView (terminal/terminal_list_view.c)
 * STRUCT FIELDS (Mirroring terminal/terminal_list_view.h)
 * const char *const *labels  borrowed labels
 * size_t count               label count
 * size_t selection           selected index
 * size_t scroll              first visible index
 * const char *marker         selected-row marker (default "> ")
 * TerminalPen normal, selected  row pens
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalListView_0(void), TerminalListView_2(labels, count)
 * Config: set/getLabels, set/getSelection, set/getScroll, set/getMarker,
 *   set/getPens
 * Navigation: bool TerminalListView_move(self, delta, visibleRows)
 * Render: TerminalListView_render(self, canvas)
 * Projections: bool TerminalListView_toString/toStringStruct(...)
 * CONTRACT: null/invalid canvas no-ops; rows past the region clip; a null label
 * renders blank; render never allocates or logs.
 * ======================================================================== */

TerminalListView TerminalListView_0(void) {
    TerminalListView view = {0};
    view.marker = TERMINAL_LIST_MARKER_DEFAULT;
    view.normal = TerminalPen_0();
    view.selected = TerminalPen_1(TERMINAL_STYLE_REVERSE);
    return view;
}

TerminalListView TerminalListView_2(const char *const *labels, size_t count) {
    TerminalListView view = TerminalListView_0();
    view.labels = (count != 0) ? labels : nullptr;
    view.count = (labels != nullptr) ? count : 0;
    return view;
}

bool TerminalListView_setLabels(TerminalListView *self, const char *const *labels, size_t count) {
    if (self == nullptr || (count != 0 && labels == nullptr)) {
        THROW("TerminalListView labels: null list");
        return false;
    }
    (*self).labels = labels;
    (*self).count = count;
    return true;
}
void TerminalListView_getLabels(const TerminalListView *self, const char *const **outLabels, size_t *outCount) {
    if (outLabels) *outLabels = self ? (*self).labels : nullptr;
    if (outCount) *outCount = self ? (*self).count : 0;
}
bool TerminalListView_setSelection(TerminalListView *self, size_t selection) {
    if (self == nullptr) { THROW("TerminalListView selection: null"); return false; }
    (*self).selection = selection;
    return true;
}
size_t TerminalListView_getSelection(const TerminalListView *self) { return self ? (*self).selection : 0; }
bool TerminalListView_setScroll(TerminalListView *self, size_t scroll) {
    if (self == nullptr) { THROW("TerminalListView scroll: null"); return false; }
    (*self).scroll = scroll;
    return true;
}
size_t TerminalListView_getScroll(const TerminalListView *self) { return self ? (*self).scroll : 0; }
bool TerminalListView_setMarker(TerminalListView *self, const char *marker) {
    if (self == nullptr) { THROW("TerminalListView marker: null"); return false; }
    (*self).marker = marker;
    return true;
}
const char *TerminalListView_getMarker(const TerminalListView *self) {
    return (self && (*self).marker) ? (*self).marker : TERMINAL_LIST_MARKER_DEFAULT;
}
void TerminalListView_setPens(TerminalListView *self, TerminalPen normal, TerminalPen selected) {
    if (self != nullptr) { (*self).normal = normal; (*self).selected = selected; }
}
void TerminalListView_getPens(const TerminalListView *self, TerminalPen *outNormal, TerminalPen *outSelected) {
    if (outNormal) *outNormal = self ? (*self).normal : TerminalPen_0();
    if (outSelected) *outSelected = self ? (*self).selected : TerminalPen_0();
}

bool TerminalListView_move(TerminalListView *self, long delta, size_t visibleRows) {
    if (self == nullptr || (*self).count == 0)
        return false;
    size_t selection = (*self).selection;
    if (delta < 0) {
        if (selection == 0)
            return false;
        selection -= (size_t) -delta;
    } else {
        size_t next = selection + (size_t) delta;
        if (next >= (*self).count)
            next = (*self).count - 1;
        if (next == selection)
            return false;
        selection = next;
    }
    (*self).selection = selection;
    if (visibleRows > 0) {
        if (selection < (*self).scroll)
            (*self).scroll = selection;
        else if (selection >= (*self).scroll + visibleRows)
            (*self).scroll = selection - visibleRows + 1;
    }
    return true;
}

void TerminalListView_render(const TerminalListView *self, TerminalCanvas *canvas) {
    if (self == nullptr || !TerminalCanvas_isValid(canvas))
        return;
    size_t rows = TerminalCanvas_getHeight(canvas);
    size_t markerLen = self ? strlen(TerminalListView_getMarker(self)) : 0;
    for (size_t row = 0; row < rows; ++row) {
        size_t index = (*self).scroll + row;
        if (index >= (*self).count)
            break;
        bool selected = index == (*self).selection;
        TerminalPen pen = selected ? (*self).selected : (*self).normal;
        if (selected)
            TerminalCanvas_text(canvas, 0, row, TerminalListView_getMarker(self), markerLen, pen);
        const char *label = (*self).labels[index];
        if (label != nullptr)
            TerminalCanvas_text(canvas, markerLen, row, label, strlen(label), pen);
    }
}

bool TerminalListView_toString(const TerminalListView *self, char *dest, size_t cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("TerminalListView projection: missing destination");
        return false;
    }
    int n = self ? snprintf(dest, cap,
        "TerminalListView{count=%zu,selection=%zu,scroll=%zu,marker=\"%s\"}",
        (*self).count, (*self).selection, (*self).scroll, TerminalListView_getMarker(self))
        : snprintf(dest, cap, "nullptr");
    bool cut = n < 0 || (size_t) n >= cap;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    if (cut)
        THROW("TerminalListView projection: truncated at %zu bytes", cap);
    return !cut;
}

bool TerminalListView_toStringStruct(const TerminalListView *self, char *dest, size_t cap, bool *outTruncated) {
    return TerminalListView_toString(self, dest, cap, outTruncated);
}
