#ifndef DETERMINANT_TERMINAL_STATUS_BAR_H
#define DETERMINANT_TERMINAL_STATUS_BAR_H

#include <stdbool.h>
#include <stddef.h>

#include "terminal/terminal_canvas.h"
#include "terminal/terminal_projection.h"
#include "c23/constructor.h"

// terminal/terminal_status_bar.h — a bare, generic one-row status line.
//
// Draws a borrowed left string and an optional borrowed right string on one row.
// The right string is drawn only when it cannot collide with the left, so a
// narrow column degrades by dropping the right text rather than overwriting.
// It carries no meaning: a channel name, a mode, a badge — all are just strings
// the caller supplies.

typedef struct TerminalStatusBar {
    const char *left;
    const char *right;   // nullable
    TerminalPen leftPen;
    TerminalPen rightPen;
} TerminalStatusBar;
typedef struct TerminalStatusBarPens { TerminalPen left; TerminalPen right; } TerminalStatusBarPens;

TerminalStatusBar TerminalStatusBar_0(void);
TerminalStatusBar TerminalStatusBar_1(const char *left);
#define TerminalStatusBar(...) CONSTRUCTOR_DISPATCH(TerminalStatusBar, __VA_ARGS__)
#define TerminalStatusBar_zero() TerminalStatusBar_0()

bool TerminalStatusBar_setLeft(TerminalStatusBar *self, const char *left);
const char *TerminalStatusBar_getLeft(const TerminalStatusBar *self);
bool TerminalStatusBar_setRight(TerminalStatusBar *self, const char *right);
const char *TerminalStatusBar_getRight(const TerminalStatusBar *self);
void TerminalStatusBar_setPens(TerminalStatusBar *self, TerminalPen leftPen, TerminalPen rightPen);
TerminalStatusBarPens TerminalStatusBar_getPens(const TerminalStatusBar *self);

void TerminalStatusBar_render(const TerminalStatusBar *self, TerminalCanvas *canvas);

TerminalProjection TerminalStatusBar_toString(const TerminalStatusBar *self);
TerminalProjection TerminalStatusBar_toStringStruct(const TerminalStatusBar *self);

#endif
