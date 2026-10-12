#ifndef DETERMINANT_COMMANDPARSER_H
#define DETERMINANT_COMMANDPARSER_H
#include "cli/command.h"
// Plain whitespace-separated ASCII tokens; no quotes, shell expansion or exec.
// Length-based form rejects embedded NUL/control/non-ASCII before allocation.
// Empty/whitespace is an ordinary no-command result, not an error.
Command *CommandParser_parse(const char *line);
Command *CommandParser_parseBytes(const char *line, size_t length);
#endif
