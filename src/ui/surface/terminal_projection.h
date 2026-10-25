#ifndef DETERMINANT_TERMINAL_PROJECTION_H
#define DETERMINANT_TERMINAL_PROJECTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct TerminalProjection {
    char text[512];
    size_t length;
    bool truncated;
} TerminalProjection;

static inline TerminalProjection TerminalProjection_format(const char *format, ...) {
    TerminalProjection projection = {0};
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(projection.text, sizeof projection.text, format, arguments);
    va_end(arguments);
    if (length < 0) {
        projection.text[0] = '\0';
        projection.truncated = true;
        return projection;
    }
    projection.truncated = (size_t) length >= sizeof projection.text;
    projection.length = projection.truncated ? sizeof projection.text - 1u : (size_t) length;
    return projection;
}

#endif
