# Determinant — R3 Terminal Toolkit Preferences

## Constitution Link

The complete [constitution](https://gist.github.com/vex-graph/4132a6c45cb6d3797c3e8eff2e94035a)
governs this repository. The binding law list is `gist.md`
(`../../../metadata/gists/preferences/gist.md`); `../../../metadata/gists/preferences/PREFERENCES.md` is its full
rationale and is never ingested whole. Read the law first, then this document, the proof
lawbook `../../../tests/test-preferences.md` (slice one law with
`./tools/prefs test "<title>"`), the evidence ledger
`../../../metadata/gists/test-checklist.md`, and the actual implementation.

## Law Index (Binding Matrix)

| Law Title | Scope | Enforcement |
| :--- | :--- | :--- |
| Terminal Ownership Boundary Law | R3 terminal rendering, widgets, parsing and input | Owner and dependency tests |
| Resident Terminal and Swappable Module Law | Terminal lifetime and reload | Real module swap, rejection preservation and teardown tests |
| Bare Output Separation Law | R2 print vs terminal application output | Quiet rendering and routed-output tests |

### Terminal Ownership Boundary Law

Determinant is the R3 toolkit for reusable terminal-cell rendering, widgets,
command parsing and caller-fed input. It borrows Vexspoke CPU and Relational
Engine storage contracts, never R1/R4/R5/R6 implementation headers. An
R1/standalone host supervises terminal acquisition, bounded event driving and
teardown through opaque handles/callbacks. Determinant owns no competing OS
process, storage or provider implementation. Coordinates are cells, not
graphical pixels; app-specific conversation, consent, tools, channels and
policies remain consumer-owned.

### Resident Terminal and Swappable Module Law

Terminal ownership and admitted state outlive reloadable view/command code.
Before swap, the host pauses admission, excludes active callers/callbacks and
validates the complete candidate ABI/state; it then publishes or rejects
atomically. Failure preserves old callable code/state; unresolved rollback blocks
resumption, and retirement leaves no callback into old code. R3 supplies
module-neutral contracts; R1 owns trusted loading and code residency. Toolkit
construction is not live Primeagen reload proof: migrate reusable terminal
pieces, retain app/session/consent state in the host, reproduce the
restart-required defect, then prove a visible view/command change in the same
production process with rollback.

Derived geometry, owned backing, initialized counts, feed positions and callback
generations change only through admission/feed/reserve/swap/reset/destruction;
safe queries exist, but no setters can forge capacity, ownership or residency.
This scoped managed accessor exception under the Conflict Triage Law and Single
Class Per File Law (Java Law) preserves lifetime truth. The host serializes
externally and retains callable code until users are excluded; atomics alone do
not make backing relocation or unloading safe. Cold growth is allowed, never
during admitted hot rendering. Input is caller-fed, with observable capacity
refusal/truncation, never blocking stdin reads.

### Bare Output Separation Law

Vexspoke retains minimal bare one-line printing for CLI diagnostics; R2 never
calls Determinant, which may consume R2 primitives. Terminal applications route
output through terminal presentation, never bare stdout over a managed screen.
Diagnostic stderr is a separate cold channel, not widget rendering. Hotcwap's
Console is the R1 lifecycle object, not the R2 printer or a Determinant widget.

## Repo-Local Extensions (managed, per the Conflict Triage Law)

### Placement and Component Tree Extension

The terminal hosts **one content panel**, a tree of **two component kinds**:
panel (container) and text (leaf).

- `TerminalAnchor` is a pure placement function returning the region *and* its
  resulting cursor. Bands (`BAND_TOP/BOTTOM/LEFT/RIGHT`) reserve slices, shrinking
  the cursor for later children; `FILL` and nine pins reserve nothing, letting
  backgrounds/overlays cover without disturbing flow. Layout is one rule:
  reserve or not.
- `TerminalPanel` owns tree, z/flow order, not content. One flavour field (not a
  class per kind): `FLEX` advances past each reserving child; `LAYERED` does not,
  resolving every child against the whole area. `TerminalPanel_paint` is **total**:
  repaint the whole tree on every call, without dirty bits or skipping.
- `TerminalText` is an addressable, one-row borrowed string with its own pen,
  optional brackets and alignment. It is an object, not an anonymous draw, so
  hover, recolouring and hit-testing can name it. Unset colours inherit from its
  panel; styles combine.
- Either kind may contain either. Adding transfers ownership to the parent;
  `remove` frees, `detach` returns ownership for reparenting, and freeing a panel
  frees its subtree. Direct self-containment is forbidden; indirect cycles are
  refused at paint time.
- **Resize is free and is the reason the tree exists.** Recompute every rectangle
  from canvas size on each paint: no cached sizes, just paint at new dimensions.
- Presentation delta is **not** the panel's job. `TerminalSurface` diffs the
  fresh grid against the last paint, emitting changed same-row runs only. Thus
  total repaint costs the host nothing on an unchanged frame. A refused run leaves
  the record unchanged, so next present redelivers that delta.
- Child identity is slot + generation; retired slots report stale IDs, never
  address recycled children. Derived geometry/growth changes only via
  add/remove/detach/anchor, never a lifetime-forging setter. Cold growth doubles;
  refusals are counted and reported once per epoch.

`TerminalButton` is interactive text (hover, click, retained handler) and the
only widget retaining a handler; every other widget returns an event and keeps
nothing.

`TerminalTreeView` is a scrollable, collapsible tree of addressable rows. A
node's subtree is a contiguous pre-order range, so folding is a jump and unfolding
is a step; visible rows are cached and recomputed only on a structural change;
ids are stable and never recycled. It draws nothing: a row is returned as data
(label, depth, folder, expanded, selected) and the caller paints it. That is what
lets one widget serve a file tree, a channel list and a settings menu. It is not a
panel child and does not replace the panel tree.

`TerminalScrollGlide` is the shared scroll motion for any viewport: per-axis
shown/target positions plus the easing between them, with a snap threshold. Input
moves the target; `step` returns the whole-cell position to paint. It owns no
cells and no clipping, so it is the motion half of a scroll panel, not the
`SCROLL` panel flavour. `step` is the frame path: one null guard, no logging and
no allocation.

`TerminalCodeView` is a borrowed one-line painter, not a lexer/language adapter.
Consumers supply ordered UTF-8 byte spans with terminal pens and optional inlay
labels anchored at source byte offsets. Determinant validates admission bounds
and paints clipped cells; virtual hints never change source bytes or span
coordinates. Consumers own parsing, semantic classification, theme policy and
backing lifetimes. This R3 toolkit borrows no other R3 language implementation.

### Source Taxonomy Extension

`src/` is organised by the capability a unit owns, not by a flat widget prefix:

- `ui/surface/` — `Terminal`, `TerminalCanvas`, `TerminalProjection`, `TerminalSurface`: the cell surface and last-paint pipeline.
- `ui/panel/` — `TerminalPanel`, `TerminalAnchor`, `TerminalFrame`: container, placement, region layout.
- `ui/text/` — `TerminalText`, `TerminalTextView`, `TerminalStatusBar`.
- `ui/list/`, `ui/tree/` — `TerminalListView`, `TerminalTreeView`.
- `ui/field/` — `TerminalInputLine` (single-line field); a multiline textarea is not implemented.
- `ui/code/` — `TerminalCodeView`; a diff panel and diff-file model are not implemented.
- `ui/markdown/` — `TerminalMarkdownView`.
- `ui/button/` — `TerminalButton`.
- `ui/image/` — `TerminalImage`.
- `ui/scroll/` — `TerminalScrollGlide`.
- `ui/theme/` — `TerminalAppearance` presets.
- `events/` — caller-fed input decoding (currently mouse); a normalised key event is not implemented.
- `format/` — `TerminalStyle` flags and `TerminalColor` packing.
- `cli/` — command parsing and log projection, outside `ui/`.

Every include names the canonical path (`ui/panel/terminal_panel.h`,
`events/terminal_input.h`, `format/terminal_color.h`); `src/terminal/` no longer
exists. A new reusable unit lands in the directory that owns its capability.
Class names keep the `Terminal` prefix — the directory is placement, not identity.
Mirrored owner tests live under `tests/determinant/<same relative directory>/`.

### Pending: frame retirement and the missing flavours

`TerminalFrame` still computes five regions, duplicating `TerminalAnchor` band
arithmetic deliberately, not as a settled design. Its regions differ from the
five anchor bands: the footer spans only the centre column; rails span the body,
not the footer row. Retirement therefore first requires migrating
`TerminalRule` and appearance presets. Until then, new layout belongs on panels
and anchors; never extend frame geometry.

`SCROLL` and `CODE` panel flavours are unimplemented: scrolling needs a real clip
and offset; code needs a gutter, so do not add either name early.
`TerminalScrollGlide` is now the reusable motion model, but it is not that flavour:
the panel that clips cells still does not exist. `TerminalCodeView` is only the
caller-fed line painter, not the `CODE` flavour or an editor. Generic `list_view`,
`text_view`, `input_line` and `status_bar` widgets are superseded by the tree but
not yet rebuilt from it. Cycle safety is proved for paint only; indirect cycles
still make teardown undefined, so the tree is acyclic by contract.

Not implemented or implied by an executed owner: panel-tree pointer/hover routing
(cold `TerminalPanel_childAt` exists, unused); bracketed chip/segment bar; tab
strip; editor view with line-number gutter; and child-array allocation-failure
injection. `TerminalTreeView` exists and its own owner proves folding, selection,
reveal and viewport clamping; it draws no rows itself, so the caller's painting is
not part of that proof. `cursor/` has no owner yet: caret movement, selection,
grapheme and wide-cell stepping are still rebuilt inside consumers such as
Semicolon. The format plane has no italic flag yet.

;;INTENTION("R3 terminal toolkit: anchor-owned placement over a two-kind component tree (panel container, addressable text leaf), a total repaint each frame so resize needs no invalidation, and a last-paint presenter that owns the delta; the frame's five regions remain a pending deliberate migration rather than a second layout system.")

## Current Proof Boundary

This lawbook states the accepted contract, not readiness. Owner tests belong in
`../../../tests/determinant/`. Primeagen integration, live reload, real terminal/
backend behavior and other platforms need their own executed evidence. No push,
interactive launch or live provider call is implied.
