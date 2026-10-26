#ifndef DETERMINANT_TERMINAL_STYLE_H
#define DETERMINANT_TERMINAL_STYLE_H

#include <stdint.h>

// Determinant cell style flags — the parallel attribute plane that rides beside
// the character grid. The plane is 16 bits because the eight low bits were spent:
// italic is slanted emphasis, a distinct axis from weight (BOLD) and decoration
// (UNDERLINE), and it is the bit the markdown renderer needs for *emphasis*.
// The toolkit NEVER names a terminal library or an ANSI
// escape: it sets host-neutral flags and the device backend maps them to its own
// attributes (ncurses A_DIM/A_BOLD/A_REVERSE, a Windows console attribute, a
// WebGL cell). Cells carry bytes; styles carry meaning. This is how an image or
// a de-emphasised background is expressible without the toolkit knowing any
// terminal's escape grammar.

#define TERMINAL_STYLE_NONE      0x00u
#define TERMINAL_STYLE_DIM       0x01u   // grey / de-emphasised
#define TERMINAL_STYLE_BOLD      0x02u
#define TERMINAL_STYLE_UNDERLINE 0x04u
#define TERMINAL_STYLE_REVERSE   0x08u   // selection / focused row
#define TERMINAL_STYLE_ACCENT    0x10u   // brand / active tab
#define TERMINAL_STYLE_MUTED     0x20u   // secondary text (rail hints)
#define TERMINAL_STYLE_ERROR     0x40u
#define TERMINAL_STYLE_USER      0x80u
#define TERMINAL_STYLE_ITALIC    0x100u  // slanted emphasis (needs the 16-bit plane)

#endif

