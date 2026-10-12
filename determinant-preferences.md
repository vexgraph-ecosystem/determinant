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

## Current Proof Boundary

This lawbook establishes the accepted contract, not implementation readiness.
Owner tests belong in `../../tests/determinant/`. Primeagen integration, live
reload, real terminal/backend behavior and other platforms require their own
executed evidence. No push, interactive launch or live provider call is implied.
