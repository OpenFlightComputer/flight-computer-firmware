#include "newline_framer.h"

#include <limits.h>

static void saturating_increment(uint32_t *value)
{
    if (*value < UINT32_MAX) {
        (*value)++;
    }
}

void newline_framer_initialize(newline_framer_t *framer)
{
    if (framer != NULL) {
        *framer = (newline_framer_t){0};
    }
}

void newline_framer_discard_current_line(newline_framer_t *framer)
{
    if (framer == NULL) {
        return;
    }

    framer->length = 0U;
    framer->discarding = true;
    saturating_increment(&framer->overflow_count);
}

void newline_framer_consume(newline_framer_t *framer,
                            const uint8_t *data,
                            size_t length,
                            newline_framer_line_callback_t line_callback,
                            void *callback_context)
{
    size_t index;

    if ((framer == NULL) || (line_callback == NULL) ||
        ((data == NULL) && (length > 0U))) {
        return;
    }

    for (index = 0U; index < length; index++) {
        const uint8_t byte = data[index];

        if (framer->discarding) {
            if (byte == (uint8_t)'\n') {
                framer->discarding = false;
            }
            continue;
        }

        if (byte == (uint8_t)'\n') {
            size_t line_length = framer->length;

            if ((line_length > 0U) &&
                (framer->line[line_length - 1U] == (uint8_t)'\r')) {
                line_length--;
            }

            line_callback(framer->line, line_length, callback_context);
            framer->length = 0U;
            continue;
        }

        if (framer->length == NEWLINE_FRAMER_MAX_LINE_LENGTH) {
            newline_framer_discard_current_line(framer);
            continue;
        }

        framer->line[framer->length] = byte;
        framer->length++;
    }
}
