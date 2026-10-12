#include "cli/command.h"
#include "primitive/string.h"
#include "nio/mem.h"
#include "oop/type.h"
#include "exception/throw.h"
#include "annotation/definition.h"
#include "annotation/overview.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** An R3 parsed command owns R2 string blocks and one raw R2 byte allocation.
 * No invented object type ID is used: the backing is bytes, not a claimed
 * Vexspoke Command identity. The copied pointer directory follows the class
 * in the allocation; ownership transfers only after complete admission. */
;;OVERVIEW
/** ========================================================================
 * CLASS: Command (cli/command.c)
 * STRUCT FIELDS (Mirroring cli/command.h)
 * uint8_t *name              owned R2 string block
 * size_t argumentCount       initialized directory extent
 * uint8_t **arguments        owned strings in inline pointer directory
 * FUNCTION REGISTRY (exports)
 * Constructors: Command *Command_0(void)
 *   Command *Command_3(uint8_t *name, uint8_t **arguments, size_t count)
 * Getters: uint8_t *Command_name(const Command *self)
 *   size_t Command_argumentCount(const Command *self)
 *   uint8_t *Command_argument(const Command *self, size_t index)
 * Lifecycle: void Command_free(Command *self)
 * Projections: bool Command_toString/Command_toStringStruct(
 *   const Command *self, char *dest, size_t cap, bool *outTruncated)
 * CONTRACT: caller-thread cold allocation; live string blocks required;
 * null/overflow/duplicate ownership reject before transfer. Failure preserves
 * input ownership. Geometry changes only by construction/free (Conflict Triage
 * Law); no setter can forge a directory extent or duplicate owned pointers.
 * Null getters return zero/null. Free null is harmless. Projections redact
 * owned strings, avoiding raw control/secret text; truncation always flagged.
 * ======================================================================== */
Command *Command_0(void) {
    uint8_t *name = String("");
    if (name == nullptr)
        return nullptr;
    Command *self = Command_3(name, nullptr, 0);
    if (self == nullptr)
        string_free(name);
    return self;
}
Command *Command_3(uint8_t *name, uint8_t **arguments, size_t count) {
    if (name == nullptr || (count != 0 && arguments == nullptr)
        || count > (UINT32_MAX - sizeof(Command)) / sizeof(uint8_t*)) {
        THROW("Command construct: null input or extent overflow");
        return nullptr;
    }
    for (size_t i = 0; i < count; ++i) {
        if (arguments[i] == nullptr || arguments[i] == name) {
            THROW("Command construct: invalid owned argument");
            return nullptr;
        }
        for (size_t j = 0; j < i; ++j)
            if (arguments[i] == arguments[j]) {
                THROW("Command construct: duplicate owned argument");
                return nullptr;
            }
    }
    size_t bytes = sizeof(Command) + count * sizeof(uint8_t*);
    Command *self = Memory_alloc(TYPE_BYTE_ARRAY, bytes);
    if (self == nullptr)
        return nullptr; // R2 owner reports/counts allocation exhaustion.
    (*self).name = name;
    (*self).argumentCount = count;
    (*self).arguments = (uint8_t**) (self + 1);
    if (count != 0)
        memcpy((*self).arguments, arguments, count * sizeof(uint8_t*));
    return self;
}
uint8_t *Command_name(const Command *self) { return self ? (*self).name : nullptr; }
size_t Command_argumentCount(const Command *self) { return self ? (*self).argumentCount : 0; }
uint8_t *Command_argument(const Command *self, size_t index) {
    if (self == nullptr || index >= (*self).argumentCount)
        return nullptr;
    return (*self).arguments[index];
}
void Command_free(Command *self) {
    if (self == nullptr)
        return;
    string_free((*self).name);
    for (size_t i = 0; i < (*self).argumentCount; ++i)
        string_free((*self).arguments[i]);
    Memory_free(self);
}
bool Command_toString(const Command *self, char *dest, size_t cap, bool *outTruncated) {
    return Command_toStringStruct(self, dest, cap, outTruncated);
}
bool Command_toStringStruct(const Command *self, char *dest, size_t cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if (dest == nullptr || cap == 0) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("Command projection: missing destination");
        return false;
    }
    int n = self ? snprintf(dest, cap, "Command{name=<owned>,argumentCount=%zu,arguments=<owned>}", (*self).argumentCount)
                 : snprintf(dest, cap, "nullptr");
    bool cut = n < 0 || (size_t) n >= cap;
    if (outTruncated != nullptr)
        *outTruncated = cut;
    if (cut)
        THROW("Command projection: truncated at %zu bytes", cap);
    return !cut;
}
