#ifndef OPENFLIGHTCOMPUTER_UINT64_DECIMAL_H
#define OPENFLIGHTCOMPUTER_UINT64_DECIMAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UINT64_DECIMAL_MAX_DIGITS 20U
#define UINT64_DECIMAL_BUFFER_CAPACITY (UINT64_DECIMAL_MAX_DIGITS + 1U)

bool uint64_decimal_format(uint64_t value,
                           size_t minimum_width,
                           char *destination,
                           size_t capacity,
                           size_t *length);

#endif
