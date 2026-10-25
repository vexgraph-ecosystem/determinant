#include "ui/surface/terminal.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include "annotation/intention.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** Terminal is resident R3 presentation state, not OS terminal acquisition.
 * The host lends a flat printable-ASCII cell grid, output callback and resident
 * state. View/command exports can change while these borrowed resources stay
 * resident. Admission pauses first; the active guard excludes reentrant swaps.
 * The host owns trust, ABI/state preparation, loaded-code lifetime and external
 * serialization. No atomics claim to pin retired code, and no OS/loader or R1
 * include exists here. This foundation does not implement Primeagen /update. */
;;OVERVIEW
/** ========================================================================
 * CLASS: Terminal (ui/surface/terminal.c)
 * STRUCT FIELDS (Mirroring ui/surface/terminal.h)
 * char *cells                    borrowed contiguous cell backing
 * size_t capacity                admitted backing bytes
 * size_t width                   columns (cells)
 * size_t height                  rows (cells)
 * DeterminantOutputFn output      borrowed host sink
 * void *hostContext              opaque resident sink state
 * void *residentState            opaque resident consumer state
 * TerminalModule module          borrowed reloadable exports
 * uint64_t generation            successful export publications
 * size_t refusalCount            epoch text capacity refusals
 * bool paused                    new module admission disabled
 * bool active                    current callback exclusion
 * SLOT RECORD: TerminalModule
 * uint32_t abi                   explicit export ABI identity
 * TerminalViewFn view            borrowed render callback
 * TerminalCommandFn command      borrowed command callback
 * TerminalKey: normalized host key vocabulary (enum, no owned state)
 * FUNCTION REGISTRY (exports)
 * Constructor: Terminal Terminal_0(void) [zero/arity chooser]
 * Admission: bool Terminal_bind(Terminal *self, char *cells, size_t capacity,
 *     size_t width, size_t height, DeterminantOutputFn output, void *hostContext)
 * Configuration: bool Terminal_setOutput(Terminal *self,
 *     DeterminantOutputFn output, void *hostContext)
 * Lifecycle: bool Terminal_pause(Terminal *self)
 *   bool Terminal_resume(Terminal *self), Terminal_free(Terminal *self)
 * Reload: bool Terminal_swap(Terminal *self, TerminalModule module,
 *     void *residentState)
 * Dispatch: bool Terminal_render(Terminal *self)
 *   bool Terminal_command(Terminal *self, const char *text, size_t length)
 * Cells: bool Terminal_text(Terminal *self, size_t x, size_t y,
 *     const char *text, size_t length)
 *   bool Terminal_clear(Terminal *self), Terminal_present(Terminal *self)
 * Queries: DeterminantOutputFn Terminal_getOutput(const Terminal *self)
 *   void *Terminal_getHostContext(const Terminal *self)
 *   void *Terminal_getResidentState(const Terminal *self)
 *   const char *Terminal_getCells(const Terminal *self)
 *   TerminalModule Terminal_getModule(const Terminal *self)
 *   size_t Terminal_getWidth(const Terminal *self)
 *   size_t Terminal_getHeight(const Terminal *self)
 *   size_t Terminal_getCapacity(const Terminal *self)
 *   uint64_t Terminal_getGeneration(const Terminal *self)
 *   size_t Terminal_exhaustionCount(const Terminal *self)
 *   bool Terminal_isPaused(const Terminal *self), Terminal_isActive(const Terminal *self)
 * Projections: TerminalProjection Terminal_toString/toStringStruct(self)
 * CONTRACT: null query defaults; cold invalid operations THROW once; hot
 * clear/present remain quiet. Text admission validates full span before copying.
 * Callback false may have changed caller cells/state: host owns candidate-state
 * preparation/rollback, not arbitrary native callback undo. Swap never invokes
 * candidate code and rejects unchanged. Render/present do not allocate. A sink
 * failure may have delivered a prefix. Free detaches borrowed state, never frees
 * it; busy rejects. All calls require host external serialization. Cold rebind
 * may grow/resize backing with active users excluded; initial capacity is not a
 * total ceiling. Zero terminal is paused; bind installs dimensions, not exports.
 * ======================================================================== */
;;INTENTION("per the Conflict Triage Law: geometry, backing, callback generation and admission change through lifecycle, not setters forging ownership")
Terminal Terminal_0(void) { return (Terminal) {.paused = true}; }
bool Terminal_bind(Terminal *self, char *cells, size_t capacity, size_t width, size_t height,
                   DeterminantOutputFn output, void *hostContext) {
    if (self == nullptr || (*self).active || !(*self).paused || cells == nullptr || output == nullptr
        || width == 0 || height == 0 || height > SIZE_MAX / width || width * height > capacity) {
        THROW("Terminal bind: invalid geometry/backing/sink or busy");
        return false;
    }
    (*self).cells = cells;
    (*self).capacity = capacity;
    (*self).width = width;
    (*self).height = height;
    (*self).output = output;
    (*self).hostContext = hostContext;
    memset(cells, ' ', width * height);
    return true;
}
bool Terminal_setOutput(Terminal *self, DeterminantOutputFn output, void *hostContext) {
    if (self == nullptr || (*self).active || output == nullptr) {
        THROW("Terminal setOutput: invalid sink or busy");
        return false;
    }
    (*self).output = output;
    (*self).hostContext = hostContext;
    return true;
}
bool Terminal_pause(Terminal *self) {
    if (self == nullptr) {
        THROW("Terminal pause: null terminal");
        return false;
    }
    (*self).paused = true;
    return !(*self).active; // stop new admission even while callback must finish
}
bool Terminal_resume(Terminal *self) {
    TerminalModule *module = self ? &(*self).module : nullptr;
    if (self == nullptr || (*self).active || (*self).cells == nullptr || (*module).view == nullptr) {
        THROW("Terminal resume: unbound or busy");
        return false;
    }
    (*self).paused = false;
    return true;
}
bool Terminal_swap(Terminal *self, TerminalModule module, void *residentState) {
    if (self == nullptr || (*self).active || !(*self).paused || module.abi != TERMINAL_MODULE_ABI
        || module.view == nullptr || module.command == nullptr || (*self).generation == UINT64_MAX) {
        THROW("Terminal swap: incompatible exports, generation overflow or busy");
        return false;
    }
    (*self).module = module;
    (*self).residentState = residentState;
    ++(*self).generation;
    return true;
}
bool Terminal_render(Terminal *self) {
    if (self == nullptr || (*self).paused || (*self).active) {
        THROW("Terminal render: null, paused or busy");
        return false;
    }
    TerminalModule *module = &(*self).module;
    (*self).active = true;
    bool ok = (*module).view((*self).residentState, self);
    (*self).active = false;
    if (!ok)
        THROW("Terminal render: view rejected");
    return ok;
}
bool Terminal_command(Terminal *self, const char *text, size_t length) {
    if (self == nullptr || (*self).paused || (*self).active || (text == nullptr && length != 0)) {
        THROW("Terminal command: invalid span, paused or busy");
        return false;
    }
    for (size_t i = 0; i < length; ++i)
        if ((unsigned char) text[i] < 32 || (unsigned char) text[i] >= 127) {
            THROW("Terminal command: invalid ASCII byte at %zu", i);
            return false;
        }
    TerminalModule *module = &(*self).module;
    (*self).active = true;
    bool ok = (*module).command((*self).residentState, text, length);
    (*self).active = false;
    if (!ok)
        THROW("Terminal command: callback rejected");
    return ok;
}
bool Terminal_text(Terminal *self, size_t x, size_t y, const char *text, size_t length) {
    if (self == nullptr || (*self).cells == nullptr || (text == nullptr && length != 0)) {
        THROW("Terminal text: invalid backing/span");
        return false;
    }
    if (y >= (*self).height || x > (*self).width || length > (*self).width - x) {
        if ((*self).refusalCount == 0)
            THROW("Terminal exhausted: request %zu cells at %zu,%zu, bound %zux%zu", length, x, y, (*self).width, (*self).height);
        if ((*self).refusalCount != SIZE_MAX)
            ++(*self).refusalCount;
        return false;
    }
    for (size_t i = 0; i < length; ++i)
        if ((unsigned char) text[i] < 32 || (unsigned char) text[i] >= 127) {
            THROW("Terminal text: invalid ASCII byte at %zu", i);
            return false;
        }
    if (length != 0)
        memcpy((*self).cells + y * (*self).width + x, text, length);
    return true;
}
bool Terminal_clear(Terminal *self) {
    if (self == nullptr || (*self).cells == nullptr)
        return false;
    memset((*self).cells, ' ', (*self).width * (*self).height);
    return true;
}
bool Terminal_present(Terminal *self) {
    if (self == nullptr || (*self).cells == nullptr || (*self).active)
        return false;
    (*self).active = true;
    bool ok = (*self).output((*self).hostContext, (*self).cells, (*self).width * (*self).height);
    (*self).active = false;
    return ok;
}
DeterminantOutputFn Terminal_getOutput(const Terminal *self) { return self ? (*self).output : nullptr; }
void *Terminal_getHostContext(const Terminal *self) { return self ? (*self).hostContext : nullptr; }
size_t Terminal_getWidth(const Terminal *self) { return self ? (*self).width : 0; }
size_t Terminal_getHeight(const Terminal *self) { return self ? (*self).height : 0; }
size_t Terminal_getCapacity(const Terminal *self) { return self ? (*self).capacity : 0; }
uint64_t Terminal_getGeneration(const Terminal *self) { return self ? (*self).generation : 0; }
size_t Terminal_exhaustionCount(const Terminal *self) { return self ? (*self).refusalCount : 0; }
bool Terminal_isPaused(const Terminal *self) { return self && (*self).paused; }
bool Terminal_isActive(const Terminal *self) { return self && (*self).active; }
const char *Terminal_getCells(const Terminal *self) { return self ? (*self).cells : nullptr; }
void *Terminal_getResidentState(const Terminal *self) { return self ? (*self).residentState : nullptr; }
TerminalModule Terminal_getModule(const Terminal *self) { return self ? (*self).module : (TerminalModule) {0}; }
bool Terminal_free(Terminal *self) {
    if (self == nullptr || (*self).active) {
        THROW("Terminal free: null or busy");
        return false;
    }
    *self = Terminal_0();
    return true;
}
TerminalProjection Terminal_toString(const Terminal *self) {
    return Terminal_toStringStruct(self);
}
TerminalProjection Terminal_toStringStruct(const Terminal *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    TerminalModule const *module = self ? &(*self).module : nullptr;
    return TerminalProjection_format("Terminal{cells=<borrowed>,capacity=%zu,width=%zu,height=%zu,output=%s,hostContext=<opaque>,residentState=<opaque>,module={abi=%u},generation=%llu,refusalCount=%zu,paused=%d,active=%d}",
        (*self).capacity, (*self).width, (*self).height, (*self).output ? "bound" : "none", (*module).abi,
        (unsigned long long) (*self).generation, (*self).refusalCount, (*self).paused, (*self).active);
}
