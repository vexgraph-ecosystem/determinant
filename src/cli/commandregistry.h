#ifndef DETERMINANT_COMMANDREGISTRY_H
#define DETERMINANT_COMMANDREGISTRY_H
#include "cli/command.h"
#include "cli/output.h"
typedef void (*CommandFn)(Command *command);
// Behaviorless borrowed row; name/code outlive registry consumers.
typedef struct CommandRegistryRow {
    const char *name;
    CommandFn target;
} CommandRegistryRow;
typedef struct CommandRegistry {
    CommandRegistryRow *rows;
    size_t capacity;
    size_t count;
    DeterminantOutputFn output;
    void *hostContext;
    size_t refusalCount;
    bool active;
} CommandRegistry;
CommandRegistry CommandRegistry_0(void);
CommandRegistry CommandRegistry_4(CommandRegistryRow *rows, size_t capacity, DeterminantOutputFn output, void *hostContext);
#define CommandRegistry(...) CONSTRUCTOR_DISPATCH(CommandRegistry, __VA_ARGS__)
#define CommandRegistry_zero() CommandRegistry_0()
// reserve copies initialized rows into disjoint larger caller backing. Not hot.
bool CommandRegistry_reserve(CommandRegistry *self, CommandRegistryRow *rows, size_t capacity);
bool CommandRegistry_register(CommandRegistry *self, const char *name, CommandFn target);
#define CommandRegistry_add CommandRegistry_register
bool CommandRegistry_execute(CommandRegistry *self, Command *command);
bool CommandRegistry_setOutput(CommandRegistry *self, DeterminantOutputFn output, void *hostContext);
DeterminantOutputFn CommandRegistry_getOutput(const CommandRegistry *self);
void *CommandRegistry_getHostContext(const CommandRegistry *self);
size_t CommandRegistry_getCount(const CommandRegistry *self);
size_t CommandRegistry_getCapacity(const CommandRegistry *self);
size_t CommandRegistry_exhaustionCount(const CommandRegistry *self);
const CommandRegistryRow *CommandRegistry_getRows(const CommandRegistry *self);
bool CommandRegistry_isActive(const CommandRegistry *self);
bool CommandRegistry_free(CommandRegistry *self);
bool CommandRegistry_toString(const CommandRegistry *self, char *dest, size_t cap, bool *outTruncated);
bool CommandRegistry_toStringStruct(const CommandRegistry *self, char *dest, size_t cap, bool *outTruncated);
#endif
