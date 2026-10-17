# Determinant — R3 Terminal Toolkit Preferences

## Constitution Link

The complete [constitution](https://gist.github.com/vex-graph/4132a6c45cb6d3797c3e8eff2e94035a)
governs this repository. Workspace source: `../../gist/PREFERENCES.md`.
Read it first, then this document, `../../tests/test-preferences.md`,
`../../gist/test-checklist.md`, and the actual implementation.

## Law Index (Binding Matrix)

| Law Title | Scope | Enforcement |
| :--- | :--- | :--- |
| Terminal Ownership Boundary Law | R3 terminal rendering, widgets, parsing and input | Owner and dependency tests |
| Resident Terminal and Swappable Module Law | Terminal lifetime and reload | Real module swap, rejection preservation and teardown tests |
| Bare Output Separation Law | R2 print vs terminal application output | Quiet rendering and routed-output tests |

### Terminal Ownership Boundary Law

Determinant is the R3 terminal toolkit. It owns reusable terminal-cell rendering,
widgets, command parsing and terminal input contracts. It borrows Vexspoke CPU
and Relational Engine storage contracts, never R1/R4/R5/R6 implementation headers.
An R1/standalone host supervises terminal acquisition, bounded event driving and
teardown through opaque handles and callbacks. No competing OS process, storage
or provider implementation belongs here. Terminal coordinates are cells, not
graphical native pixels. App-specific conversation, consent, tools, channels
and policies remain consumers.

### Resident Terminal and Swappable Module Law

Terminal ownership and admitted state outlive reloadable view/command code.
The host pauses module admission, excludes active callers/callbacks, validates
the complete candidate ABI and state, then publishes or rejects atomically.
Candidate failure preserves the old callable code and state; unresolved rollback
blocks resumption. No callback into retired code survives retirement. R3 supplies
module-neutral contracts, while R1 owns trusted loading and code residency.
Construction of a toolkit is not proof of live Primeagen reload. The follow-up
must migrate Primeagen's reusable terminal pieces, retain app/session/consent
state in its host, reproduce the restart-required defect, and prove a visible
view/command change in the same production terminal process with rollback.

Derived geometry, owned backing, initialized counts, feed positions and callback
generations change only through admission/feed/reserve/swap/reset/destruction.
They have safe queries, not setters that forge capacity, ownership or code
residency. This scoped managed accessor exception under the Conflict Triage Law
and Single Class Per File Law (Java Law) preserves lifetime truth. The host
supplies external serialization and retains callable code until all users are
excluded; atomics alone would not make backing relocation or code unloading safe.
Cold storage growth is allowed, never during admitted hot rendering. Input is
caller-fed with observable capacity refusal/truncation, not blocking stdin reads.

### Bare Output Separation Law

Vexspoke retains a minimal bare one-line print API for ordinary CLI diagnostics.
R2 never calls Determinant. Determinant may consume R2 primitives, just as a
convenience API delegates to its lower-level calculation API. A terminal
application routes application output through its terminal presentation seam;
bare stdout printing must not overwrite a managed screen. Diagnostic stderr
remains a separate cold channel, not widget rendering. Hotcwap's Console is
the R1 lifecycle object, not the R2 bare printer or a Determinant terminal widget.

## Repo-Local Extensions (managed, per the Conflict Triage Law)

### Placement and Component Tree Extension

The terminal hosts **one content panel**, and that panel is a tree of **two kinds
of component**: a panel (container) and a text (leaf).

- `TerminalAnchor` owns placement as a pure function: it returns the placed
  region *and* the cursor it leaves. Bands (`BAND_TOP/BOTTOM/LEFT/RIGHT`) reserve
  their slice so later children see a smaller cursor; `FILL` and the nine pins
  reserve nothing, so a background or an overlay covers the area without
  disturbing the flow. One rule — reserve or not — is the whole layout model.
- `TerminalPanel` owns the tree, its z-order and its flow order, and nothing about
  content. Its flavour is ONE field, not one class per kind: `FLEX` advances the
  cursor past each reserving child, `LAYERED` never advances it so every child
  resolves against the whole area. `TerminalPanel_paint` is **total by contract**:
  the whole tree repaints on every call, with no dirty bits and no skipping.
- `TerminalText` is the addressable leaf: a borrowed string on one row, with its
  own pen, optional bracket glyphs and an alignment. Text is an object rather than
  an anonymous draw because hover, recolouring and hit-testing all need something
  to name. Colours a text leaves unset are inherited from the panel it sits on;
  styles combine.
- A child is a panel or a text, and either may be a child of either. Adding
  transfers ownership to the parent; `remove` frees, `detach` returns ownership for
  re-parenting, and freeing a panel frees its whole subtree. A panel cannot contain
  itself directly, and an indirect cycle is refused at paint time.
- **Resize is free and is the reason the tree exists.** Every rectangle is
  recomputed from the canvas size on every paint, so nothing caches a size and a
  resize is just the next paint at new dimensions.
- The presentation delta is **not** the panel's job. `TerminalSurface` diffs the
  freshly painted grid against the last paint and emits only changed same-row
  runs; a total repaint therefore still costs the host nothing on an unchanged
  frame. A refused run leaves the record untouched so the next present
  re-delivers the same delta.
- Child identities are slot + generation. A retired slot reports a stale id
  instead of addressing a recycled child; derived geometry and growth change only
  through add/remove/detach/anchor operations, never through a setter that could
  forge lifetime. Cold growth doubles; refusals are counted and reported once per
  epoch.

`TerminalButton` is a text with interaction (hover, click, a retained handler),
and it is the only place a handler is retained — every other widget returns an
event and keeps nothing.

### Pending: frame retirement and the missing flavours

`TerminalFrame` still computes its own five regions, so band arithmetic is
duplicated with `TerminalAnchor`. This is a stated, deliberate duplication, not a
settled design: the frame's five regions are **not** the same five anchor bands —
its footer spans only the centre column, and its rails span the body but not the
footer row — so retiring it means migrating `TerminalRule` and the appearance
presets first. Until that mapping lands, new layout work belongs on panels and
anchors; never extend the frame's geometry.

`SCROLL` and `CODE` panel flavours are not implemented: scrolling needs a real
clip and offset, and a code field needs a gutter, so neither is a name to add
early. The generic `list_view`, `text_view`, `input_line` and `status_bar` widgets
are superseded by the tree but not yet rebuilt from it. Cycle safety is proved for
paint only: an indirect cycle still makes teardown undefined, so the tree is
acyclic by contract.

Not implemented, and not implied by any executed owner: pointer/hover routing
across the tree (the cold `TerminalPanel_childAt` primitive exists, nothing uses
it yet), the bracketed chip/segment bar, a hierarchical tree view, a tab strip, an
editor view with a line-number gutter, and child-array allocation-failure
injection.

;;INTENTION("R3 terminal toolkit: anchor-owned placement over a two-kind component tree (panel container, addressable text leaf), a total repaint each frame so resize needs no invalidation, and a last-paint presenter that owns the delta; the frame's five regions remain a pending deliberate migration rather than a second layout system.")

## Current Proof Boundary

This lawbook establishes the accepted contract, not implementation readiness.
Owner tests belong in `../../tests/determinant/`. Primeagen integration, live
reload, real terminal/backend behavior and other platforms require their own
executed evidence. No push, interactive launch or live provider call is implied.
