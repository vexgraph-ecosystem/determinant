# Determinant

## Current State

Determinant is an R3 terminal toolkit foundation, extracted from Vexspoke's CLI.
Implemented source includes owned parsed commands, whitespace command parsing,
an explicit caller-backed command registry, caller-fed scanner, sink-routed log
presentation, a caller-owned ASCII cell grid, normalized key/click button input,
and a resident terminal with replaceable view/command exports. No OS terminal,
worker, sockets, loader or app/session policy is implemented here.

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
