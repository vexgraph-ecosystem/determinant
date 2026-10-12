#ifndef DETERMINANT_TERMINAL_H
#define DETERMINANT_TERMINAL_H
#include "cli/output.h"
#include "c23/constructor.h"
#include <stdint.h>
// Wire/ABI identity; changes require a new explicit admission version.
#define TERMINAL_MODULE_ABI 1u
// Host-normalized key vocabulary, independent of OS/ncurses numeric values.
typedef enum TerminalKey {
    TERMINAL_KEY_NONE, TERMINAL_KEY_ENTER, TERMINAL_KEY_SPACE,
    TERMINAL_KEY_ESCAPE, TERMINAL_KEY_UP, TERMINAL_KEY_DOWN
} TerminalKey;
typedef struct Terminal Terminal;
typedef bool (*TerminalViewFn)(void *residentState, Terminal *terminal);
typedef bool (*TerminalCommandFn)(void *residentState, const char *text, size_t length);
// Behaviorless module export slot. Contains no ownership or loader behavior.
typedef struct TerminalModule {
    uint32_t abi;
    TerminalViewFn view;
    TerminalCommandFn command;
} TerminalModule;
struct Terminal {
    char *cells;
    size_t capacity;
    size_t width;
    size_t height;
    DeterminantOutputFn output;
    void *hostContext;
    void *residentState;
    TerminalModule module;
    uint64_t generation;
    size_t refusalCount;
    bool paused;
    bool active;
};
Terminal Terminal_0(void);
#define Terminal(...) CONSTRUCTOR_DISPATCH(Terminal, __VA_ARGS__)
#define Terminal_zero() Terminal_0()
// All spans live/exclusive; geometry multiplication validated cold.
bool Terminal_bind(Terminal *self, char *cells, size_t capacity, size_t width, size_t height,
                   DeterminantOutputFn output, void *hostContext);
bool Terminal_setOutput(Terminal *self, DeterminantOutputFn output, void *hostContext);
DeterminantOutputFn Terminal_getOutput(const Terminal *self);
void *Terminal_getHostContext(const Terminal *self);
bool Terminal_pause(Terminal *self);
bool Terminal_resume(Terminal *self);
// Host validates/trusts loaded code and state before this admission, excludes all
// callbacks, retains candidate/old code until replaced/detached. No dlopen here.
// Null callbacks, wrong ABI, unpaused/busy or exhausted generation reject unchanged.
bool Terminal_swap(Terminal *self, TerminalModule module, void *residentState);
bool Terminal_render(Terminal *self);
bool Terminal_command(Terminal *self, const char *text, size_t length);
// Cold printable-ASCII admission; whole text fits one row or rejects unchanged.
bool Terminal_text(Terminal *self, size_t x, size_t y, const char *text, size_t length);
// Hot admitted clear/present, no allocation/logging. Host sink must be bounded.
bool Terminal_clear(Terminal *self);
bool Terminal_present(Terminal *self);
size_t Terminal_getWidth(const Terminal *self);
size_t Terminal_getHeight(const Terminal *self);
size_t Terminal_getCapacity(const Terminal *self);
uint64_t Terminal_getGeneration(const Terminal *self);
size_t Terminal_exhaustionCount(const Terminal *self);
bool Terminal_isPaused(const Terminal *self);
bool Terminal_isActive(const Terminal *self);
const char *Terminal_getCells(const Terminal *self);
void *Terminal_getResidentState(const Terminal *self);
TerminalModule Terminal_getModule(const Terminal *self);
bool Terminal_free(Terminal *self);
bool Terminal_toString(const Terminal *self, char *dest, size_t cap, bool *outTruncated);
bool Terminal_toStringStruct(const Terminal *self, char *dest, size_t cap, bool *outTruncated);
#endif
