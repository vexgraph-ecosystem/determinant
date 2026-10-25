#ifndef DETERMINANT_TERMINAL_TREE_VIEW_H
#define DETERMINANT_TERMINAL_TREE_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/surface/terminal_projection.h"
#include "c23/constructor.h"

// terminal/terminal_tree_view.h — a scrollable, collapsible tree of addressable rows.
//
// A list is a tree with one level of depth, and a file browser is a list whose rows can
// be folded away. Both are the same widget, and this is it: nodes, which ones are
// folded, which one is selected, where the viewport is, and which row the pointer is
// over. It draws NOTHING — a row is handed back as data (label, depth, folded, selected)
// and the caller paints it with whatever it already paints things with. That is the same
// split the surface/presenter pair uses, and it is what lets one widget serve a file
// tree, a channel list and a settings menu.
//
// WHAT IT OWNS. The structure and every question you can ask about it, so no caller ever
// walks it itself: the visible rows are computed once per structural change and cached,
// and a lookup by row or by viewport offset is an array read. Scrolling is clamped here
// too, so a caller cannot store an offset past the end and wonder why nothing moved.
//
// WHAT IT DOES NOT OWN. The labels — they are borrowed, exactly like every other label
// in this toolkit — and the meaning of a row. "Folder" is a flag that says a node has
// foldable children; whether that is a directory, a namespace or a collapsed section is
// the caller's business.
//
// IDENTITY. A node id is stable for the life of the node and is NEVER reused, so a stale
// id simply stops being found rather than quietly addressing somebody else's row. That
// matters more here than in a panel: a tree is edited while it is being displayed.

#define TERMINAL_TREE_CHILDREN_DEFAULT 16u
#define TERMINAL_TREE_VIEWPORT_DEFAULT 8u

// No node. Zero is never a live id, so it is the natural "nothing here".
#define TERMINAL_TREE_NO_NODE ((TerminalTreeNodeId) 0u)
// No row, for the queries that answer a row.
#define TERMINAL_TREE_NO_ROW ((size_t) -1)

typedef uint32_t TerminalTreeNodeId;

// One visible row, as the caller needs it to paint. Returned BY VALUE: this is the whole
// reason the caller never walks the tree.
//
// SLOT RECORD: TerminalTreeRow (no behaviour of its own)
typedef struct TerminalTreeRow {
    TerminalTreeNodeId id;   // the node this row shows
    const char *label;       // borrowed label, never copied
    size_t depth;            // 0 at the top level, one more per level of nesting
    bool folder;             // has foldable children
    bool expanded;           // a folder that is NOT folded
    bool selected;           // the tree's current selection
} TerminalTreeRow;

// One node, in PRE-ORDER: a node's whole subtree is the contiguous range
// [index, index + subtreeCount), so folding is a jump rather than a walk.
//
// SLOT RECORD: TerminalTreeNode (no behaviour of its own)
typedef struct TerminalTreeNode {
    TerminalTreeNodeId id;   // stable identity, never reused
    int32_t parent;          // parent's index in the node array, or NO_PARENT
    size_t subtreeCount;     // nodes in this subtree, including this one
    size_t depth;            // 0 at the top level
    bool folder;             // has foldable children
    bool expanded;           // unfolded (only meaningful for a folder)
    const char *label;       // borrowed
} TerminalTreeNode;

typedef struct TerminalTreeView {
    TerminalTreeNode *nodes;      // owned pre-order node array
    size_t nodeCount;             // live nodes
    size_t nodeCapacity;          // admitted node slots
    uint32_t *rows;               // owned visible-row array (node indices)
    size_t visibleCount;          // rows currently visible
    size_t rowsCapacity;          // admitted row slots
    TerminalTreeNodeId nextId;    // identity source, never rewound
    TerminalTreeNodeId selected;  // NO_NODE when nothing is selected
    size_t scrollRow;             // first visible row
    size_t viewportRows;          // rows the caller can show
    size_t nodeRefusals;          // node-growth refusals in this epoch
    size_t rowRefusals;           // row-growth refusals in this epoch
} TerminalTreeView;

TerminalTreeView *TerminalTreeView_0(void);
#define TerminalTreeView(...) CONSTRUCTOR_DISPATCH(TerminalTreeView, __VA_ARGS__)
#define TerminalTreeView_zero() TerminalTreeView_0()

// Frees the view and everything it owns. Null is harmless. Labels are borrowed and are
// NOT freed, here or anywhere else.
void TerminalTreeView_free(TerminalTreeView *self);

// --- structure -------------------------------------------------------------------
// Add a node as the LAST child of `parent` (TERMINAL_TREE_NO_NODE for a top-level node).
// Returns NO_NODE when refused (a null view, an unknown parent, or exhausted storage),
// and a refused add changes nothing. The label is borrowed and must outlive the node.
//
// A node declared a FOLDER arrives COLLAPSED, which is what a caller wants when it is
// building a directory tree: five levels do not land on the screen at once. Opening a
// level is explicit — setExpanded, or reveal to open the path to one node.
//
// A node that then gains its FIRST child while declared a LEAF is promoted to a folder
// and opened. That asymmetry is deliberate: a caller that just added a child plainly
// wants it seen, and the alternative — rows that exist but are invisible because a flag
// elsewhere still says "leaf" — is a silent trap rather than a design.
TerminalTreeNodeId TerminalTreeView_addChild(TerminalTreeView *self, TerminalTreeNodeId parent,
                                             const char *label, bool folder);
// Re-point a node's label. Refused for an unknown id.
bool TerminalTreeView_setLabel(TerminalTreeView *self, TerminalTreeNodeId id, const char *label);
// Remove a node and its whole subtree. Refused for an unknown id.
bool TerminalTreeView_remove(TerminalTreeView *self, TerminalTreeNodeId id);
// Drop every node, keeping the storage. The selection and the scroll offset are cleared
// with it — a refresh must not leave a selection pointing into the tree that just went
// away, and ids are never reused so a stale one could never be caught by accident.
void TerminalTreeView_clear(TerminalTreeView *self);

// --- folding ---------------------------------------------------------------------
// Fold or unfold a node. Refused for an unknown id. A node that is not a folder has
// nothing to fold; it is not an error, it is simply unchanged.
bool TerminalTreeView_setExpanded(TerminalTreeView *self, TerminalTreeNodeId id, bool expanded);
// Returns whether the node is expanded AFTER the toggle.
bool TerminalTreeView_toggle(TerminalTreeView *self, TerminalTreeNodeId id);
bool TerminalTreeView_isExpanded(const TerminalTreeView *self, TerminalTreeNodeId id);
bool TerminalTreeView_isFolder(const TerminalTreeView *self, TerminalTreeNodeId id);
// Unfold every ancestor of a node, so it becomes reachable. Returns false for an unknown
// id. This is "reveal the file that is open" without the caller knowing the tree.
bool TerminalTreeView_reveal(TerminalTreeView *self, TerminalTreeNodeId id);

// --- selection -------------------------------------------------------------------
bool TerminalTreeView_setSelected(TerminalTreeView *self, TerminalTreeNodeId id);
// NO_NODE clears the selection, which is a legitimate state rather than an error.
TerminalTreeNodeId TerminalTreeView_getSelected(const TerminalTreeView *self);
bool TerminalTreeView_isSelected(const TerminalTreeView *self, TerminalTreeNodeId id);

// --- viewport --------------------------------------------------------------------
size_t TerminalTreeView_visibleCount(const TerminalTreeView *self);
bool TerminalTreeView_setViewportRows(TerminalTreeView *self, size_t rows);
size_t TerminalTreeView_viewportRows(const TerminalTreeView *self);
size_t TerminalTreeView_scrollRow(const TerminalTreeView *self);
size_t TerminalTreeView_maxScrollRow(const TerminalTreeView *self);
// Scroll, clamped to [0, maxScrollRow]. Returns the row actually scrolled to, so a
// caller that has run off the end can see where it landed instead of guessing.
size_t TerminalTreeView_setScrollRow(TerminalTreeView *self, size_t row);
size_t TerminalTreeView_scrollBy(TerminalTreeView *self, long rows);

// --- rows ------------------------------------------------------------------------
// The row at an absolute index. An out-of-range row is reported as an empty row rather
// than read: a caller painting a stale row must not be able to walk off the end.
TerminalTreeRow TerminalTreeView_rowAt(const TerminalTreeView *self, size_t row);
// The node under a row OFFSET from the top of the viewport — the one call a pointer
// event needs. NO_NODE means the offset is past the last row: a click on empty space
// below the tree is not a click on a row.
TerminalTreeNodeId TerminalTreeView_nodeAtOffset(const TerminalTreeView *self, size_t offset);
// The absolute row a node is displayed at, or NO_ROW when it is hidden inside a folded
// parent. A selected row that is folded away is still selected; it is simply not shown.
size_t TerminalTreeView_rowOf(const TerminalTreeView *self, TerminalTreeNodeId id);

// --- queries ---------------------------------------------------------------------
bool TerminalTreeView_isNode(const TerminalTreeView *self, TerminalTreeNodeId id);
const char *TerminalTreeView_getLabel(const TerminalTreeView *self, TerminalTreeNodeId id);
size_t TerminalTreeView_depthOf(const TerminalTreeView *self, TerminalTreeNodeId id);
// DIRECT children of a node.
size_t TerminalTreeView_childCountOf(const TerminalTreeView *self, TerminalTreeNodeId id);
// The whole subtree below a node, its own row included. This is the number the pre-order
// layout is built on, exposed so a caller can size a fold without walking.
size_t TerminalTreeView_subtreeCountOf(const TerminalTreeView *self, TerminalTreeNodeId id);
size_t TerminalTreeView_nodeCount(const TerminalTreeView *self);
size_t TerminalTreeView_capacity(const TerminalTreeView *self);
size_t TerminalTreeView_exhaustionCount(const TerminalTreeView *self);
void TerminalTreeView_clearExhaustion(TerminalTreeView *self);

TerminalProjection TerminalTreeView_toString(const TerminalTreeView *self);
TerminalProjection TerminalTreeView_toStringStruct(const TerminalTreeView *self);

#endif
