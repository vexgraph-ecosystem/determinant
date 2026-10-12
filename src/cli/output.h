#ifndef DETERMINANT_OUTPUT_H
#define DETERMINANT_OUTPUT_H
#include <stddef.h>
#include <stdbool.h>
// Borrowed callback/context; host retains callable code and context until all
// users detach. Callback accepts one literal bounded text span, never ANSI.
// Returns synchronously within host budget; false may mean partial sink output.
typedef bool (*DeterminantOutputFn)(void *hostContext, const char *text, size_t length);
#endif
