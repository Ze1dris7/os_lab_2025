#ifndef LAB6_COMMON_H
#define LAB6_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Requires mod > 0.
uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod);
bool ParseUInt64(const char *text, uint64_t *value);
bool SendAll(int fd, const void *buffer, size_t size);

// Returns 1 after reading size bytes, 0 for EOF before any bytes, or -1 on error.
int RecvAll(int fd, void *buffer, size_t size);

#endif
