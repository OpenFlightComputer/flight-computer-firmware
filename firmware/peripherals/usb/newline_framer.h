#ifndef OPENFLIGHTCOMPUTER_NEWLINE_FRAMER_H
#define OPENFLIGHTCOMPUTER_NEWLINE_FRAMER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NEWLINE_FRAMER_MAX_LINE_LENGTH 256U

typedef void (*newline_framer_line_callback_t)(const uint8_t *line,
                                               size_t length,
                                               void *context);

typedef struct {
    uint8_t line[NEWLINE_FRAMER_MAX_LINE_LENGTH];
    size_t length;
    uint32_t overflow_count;
    bool discarding;
} newline_framer_t;

void newline_framer_initialize(newline_framer_t *framer);
void newline_framer_discard_current_line(newline_framer_t *framer);
void newline_framer_consume(newline_framer_t *framer,
                            const uint8_t *data,
                            size_t length,
                            newline_framer_line_callback_t line_callback,
                            void *callback_context);

#endif
