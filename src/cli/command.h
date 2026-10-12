#ifndef DETERMINANT_COMMAND_H
#define DETERMINANT_COMMAND_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "c23/constructor.h"
typedef struct Command {
    uint8_t *name;
    size_t argumentCount;
    uint8_t **arguments;
} Command;
// Takes live R2 string blocks only on success; caller retains on failure.
// Arguments must be distinct owned blocks, distinct from name. External exclusion.
Command *Command_0(void);
Command *Command_3(uint8_t *name, uint8_t **arguments, size_t count);
#define Command(...) CONSTRUCTOR_DISPATCH(Command, __VA_ARGS__)
#define Command_zero() Command_0()
uint8_t *Command_name(const Command *self);
size_t Command_argumentCount(const Command *self);
uint8_t *Command_argument(const Command *self, size_t index);
void Command_free(Command *self);
bool Command_toString(const Command *self, char *dest, size_t cap, bool *outTruncated);
bool Command_toStringStruct(const Command *self, char *dest, size_t cap, bool *outTruncated);
#endif
