#ifndef DETERMINANT_TERMINAL_LIST_VIEW_H
#define DETERMINANT_TERMINAL_LIST_VIEW_H

#include <stdbool.h>
#include <stddef.h>

#include "terminal/terminal_canvas.h"
#include "terminal/terminal_projection.h"
#include "c23/constructor.h"

// terminal/terminal_list_view.h — a bare, generic list.
//
// Renders borrowed labels into a region, one per row, with a scroll offset and a
// single selection marker. It carries no meaning: a list of channels, a list of
// users, a command palette and a settings menu are all the same widget with
// different labels. The caller owns the labels and the pens.

#define TERMINAL_LIST_MARKER_DEFAULT "> "

typedef struct TerminalListView {
    const char *const *labels;   // borrowed labels
    size_t count;
    size_t selection;            // selected index (may be >= count: nothing marked)
    size_t scroll;               // first visible index
    const char *marker;          // selected-row marker (default "> ")
    TerminalPen normal;          // unselected row pen
    TerminalPen selected;        // selected row pen (marker + label)
} TerminalListView;
typedef struct TerminalListViewLabels { const char *const *data; size_t count; } TerminalListViewLabels;
typedef struct TerminalListViewPens { TerminalPen normal; TerminalPen selected; } TerminalListViewPens;

TerminalListView TerminalListView_0(void);
TerminalListView TerminalListView_2(const char *const *labels, size_t count);
#define TerminalListView(...) CONSTRUCTOR_DISPATCH(TerminalListView, __VA_ARGS__)
#define TerminalListView_zero() TerminalListView_0()

bool TerminalListView_setLabels(TerminalListView *self, const char *const *labels, size_t count);
TerminalListViewLabels TerminalListView_getLabels(const TerminalListView *self);
bool TerminalListView_setSelection(TerminalListView *self, size_t selection);
size_t TerminalListView_getSelection(const TerminalListView *self);
bool TerminalListView_setScroll(TerminalListView *self, size_t scroll);
size_t TerminalListView_getScroll(const TerminalListView *self);
bool TerminalListView_setMarker(TerminalListView *self, const char *marker);
const char *TerminalListView_getMarker(const TerminalListView *self);
void TerminalListView_setPens(TerminalListView *self, TerminalPen normal, TerminalPen selected);
TerminalListViewPens TerminalListView_getPens(const TerminalListView *self);
// Move the selection within [0, count) and keep it visible; false at an edge.
bool TerminalListView_move(TerminalListView *self, long delta, size_t visibleRows);

void TerminalListView_render(const TerminalListView *self, TerminalCanvas *canvas);

TerminalProjection TerminalListView_toString(const TerminalListView *self);
TerminalProjection TerminalListView_toStringStruct(const TerminalListView *self);

#endif
