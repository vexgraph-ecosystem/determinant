#ifndef DETERMINANT_SCANNER_H
#define DETERMINANT_SCANNER_H
#include "c23/constructor.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
typedef struct Scanner {
    char *bytes;
    size_t capacity;
    size_t length;
    size_t position;
    size_t refusalCount;
    bool ended;
} Scanner;
Scanner Scanner_0(void);
Scanner Scanner_2(char *bytes, size_t capacity);
#define Scanner(...) CONSTRUCTOR_DISPATCH(Scanner, __VA_ARGS__)
#define Scanner_zero() Scanner_0()
// Append whole span or reject unchanged. EOF is explicitly caller-fed. No stdin.
bool Scanner_feed(Scanner *self, const char *bytes, size_t length, bool ended);
bool Scanner_reserve(Scanner *self, char *bytes, size_t capacity);
bool Scanner_hasNextLine(const Scanner *self);
// Returned live R2 string belongs to caller; pending and EOF both return null.
uint8_t *Scanner_nextLine(Scanner *self);
uint8_t *Scanner_nextWord(Scanner *self);
void Scanner_reset(Scanner *self);
size_t Scanner_getLength(const Scanner *self);
size_t Scanner_getCapacity(const Scanner *self);
size_t Scanner_getPosition(const Scanner *self);
bool Scanner_isEnded(const Scanner *self);
size_t Scanner_exhaustionCount(const Scanner *self);
const char *Scanner_getBytes(const Scanner *self);
bool Scanner_toString(const Scanner *self, char *dest, size_t cap, bool *outTruncated);
bool Scanner_toStringStruct(const Scanner *self, char *dest, size_t cap, bool *outTruncated);
#endif
