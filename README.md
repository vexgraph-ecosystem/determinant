# Determinant

## Current State

Determinant is an R3 terminal toolkit foundation, extracted from Vexspoke's CLI.
Implemented source includes owned parsed commands, whitespace command parsing,
an explicit caller-backed command registry, caller-fed scanner, sink-routed log
presentation, and a resident terminal with replaceable view/command exports.

On top of that sits a **bare** terminal-UI toolkit — geometry and widgets only,
with no application content:

- `terminal_canvas` — a clipped rectangular view over a caller-owned cell grid of
  four parallel planes (glyph, style, **fg**, **bg**); a `TerminalPen` carries
  style + colours, and colours are packed `0xRRGGBBAA` (the Strict Color Law).
- `terminal_style` / `terminal_color` — host-neutral attribute flags and the
  colour packing helpers; the toolkit never emits an escape.
- `terminal_image` — rasterise borrowed **RGBA8 pixels into cells**: box-sample,
  luminance→glyph ramp, per-cell colour. The "image → canvas" capability.
- `terminal_frame` — a bare region layout (header / left / centre / right /
  footer) plus its own rules and an optional borrowed background image. Rules are
  a **connector** glyph set (junctions + optional stripe), not a bare `-`/`|`.
- `terminal_appearance` — named look presets (connector glyphs + palette) the app
  lists, picks and persists: `ascii-solid`, `ascii-striped`, `ascii-dashed`,
  `unicode-light`, `unicode-emdash`, `unicode-double`. Glyphs are codepoints, so
  real box drawing works. The toolkit stores no preference; the app owns that.
- `terminal_list_view`, `terminal_text_view`, `terminal_input_line`,
  `terminal_status_bar` — generic widgets holding no meaning: channels, users,
  commands and settings are all the same list with different labels.
- `terminal_anchor` — the **placement vocabulary**: `FILL`, four reserving
  `BAND`s (top/bottom/left/right) and nine `PIN`s, resolved as a pure function
  that returns the placed region *and* the cursor it leaves. It owns the band
  math: a band reserves its slice so the next layer sees a smaller cursor, while
  a fill or a pin reserves nothing, so a background or an overlay can cover the
  panel without disturbing the flow.
- `terminal_panel` — the **layered panel**. A growable stack of anchored layers,
  each with a foreground/background/style pen, an optional borrowed image and an
  opaque draw callback. It paints **every visible layer on every call** — full
  repaint by contract, no dirty bits and no skipping — and hands each painter the
  region it resolved to, so nesting is simply a painter calling
  `TerminalPanel_paint` on a child panel. The panel owns its layer array and no
  content; the application owns every panel it created.
- `terminal_surface` — the **last-paint presenter**. It diffs the freshly painted
  grid against the record of what the host was last told and emits only the cells
  that changed, as contiguous same-row runs through an injected run sink. This is
  what makes a total repaint cheap: an unchanged frame hands the host nothing at
  all, and a failed sink leaves the record untouched so the next present
  re-delivers the same delta.
- `terminal` / `terminal_button` — the resident host state with replaceable
  view/command exports, and a normalized key/click button.

The composition model is therefore: the terminal hosts **one content panel**, and
that panel is a layered panel. `TerminalAnchor` decides where each layer lands,
`TerminalPanel` paints them all every frame, and `TerminalSurface` decides what
actually reaches the host. Nothing here knows a chat, a file tree or an editor;
those are the application's layers.

Nothing here knows about chats, users, badges, personalities or `>w<` art — those
are application content drawn *through* the toolkit by the app (see the
`test(...)` owners). No OS terminal, worker, sockets, loader or app/session policy
is implemented here.

The independent owner runner is `python3 tests/determinant/run.py` from the
workspace root. Proof is scoped to the actually executed owners and platform;
source extraction and code-model indexing are not production readiness.

## Ownership and usage

Public includes are `cli/*.h` and `terminal/*.h`. Supply Determinant's `src`,
Vexspoke's `src` and Relational Engine's public roots; runtime builds borrow the
real R2 implementation, never copied allocators. The workspace's `./tools/b`
registers the library and owners. The independent runner downloads nothing.

`Console` remains Vexspoke R2 bare cold one-line printing. Determinant has no
Console class and never prints application text directly to stdout. Presentation
is an injected `DeterminantOutputFn`; the host maps its literal spans/cells into
its own terminal backend. CLI diagnostics use the separate cold stderr channel.
Scanner input is fed by the caller, not blocking `getchar`. Registry rows and
scanner backing can grow through cold reserve with active users excluded.

The terminal host borrows cells, output and resident application state. Pause
admission, finish/exclude active users, validate/trust the complete candidate
outside this toolkit, call `Terminal_swap`, then resume only on resolved success
or rejection. Keep old code mapped until all callbacks are excluded. Rejected
ABI/callback/generation admission preserves old exports/state; this callback seam
does not undo arbitrary native code or implement a trusted disk update protocol.

## Required Primeagen follow-up

Primeagen was **not changed or fixed by this extraction**. Its consumer cycle must
first reproduce the reported restart-required `/update` defect, migrate reusable
terminal pieces, then change a view/command in the same production terminal
process with rejection/rollback while terminal, conversation, draft, consent,
session pipes and host state remain resident. No live model call or visual
acceptance is implied. Hotcwap remains the R1 trusted loading owner.

## Scope and Limitations

Placement has one general owner (`terminal_anchor`) and one composition owner
(`terminal_panel`), but `terminal_frame` still computes its own five regions, so
band arithmetic is currently duplicated. That duplication is stated rather than
settled: the frame's five regions are **not** the same five bands the anchor flow
produces — its footer spans only the centre column, and its rails span the body
but not the footer row — so folding it in needs a considered mapping, not a
substitution. Until that mapping lands, new layout work belongs on anchors and
panels, never on the frame's geometry.

Not implemented, and not implied by anything tested here: hover or pointer
hit-testing across the layer stack, the bracketed chip/segment bar the title and
status lines need, a hierarchical tree view, a tab strip, and an editor view with
a line-number gutter. A panel layer borrows an image through
`TerminalPanel_setImage`; it never owns one. Allocation-failure injection during
layer growth is unreachable in-process and is therefore unproved, as is a real
nested paint driven by a child panel's draw callback (the owner proves the
resolved regions, not the callback chain).

ASCII cells are the current format: no Unicode/grapheme shaping, ANSI decoder,
ncurses backend, physical terminal or cross-platform runtime proof is claimed.
Cells/rows/input are borrowed live exclusive spans; external serialization is
required. No stale-pointer validation or internal thread-safety is promised.
R2 owns allocation exhaustion for parsed commands. Host-owned buffers have
observable counted refusals and can be replaced cold; no permanent initial
capacity ceiling is imposed. Log reads are synchronous, stable-file operations;
the underlying parser cannot cancel mid-read. Sink failure may have delivered a
prefix. Full fault injection, leak/resource accounting and Primeagen integration
remain separate proof obligations. No Tier-1 backend-trust promotion is implied.
