#include "newline_framer.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t lines[4][NEWLINE_FRAMER_MAX_LINE_LENGTH];
    size_t lengths[4];
    size_t count;
} capture_t;

static void capture_line(const uint8_t *line, size_t length, void *context)
{
    capture_t *capture = context;

    assert(capture->count < 4U);
    if (length > 0U) {
        memcpy(capture->lines[capture->count], line, length);
    }
    capture->lengths[capture->count] = length;
    capture->count++;
}

static void partial_crlf_and_multiple_lines_are_framed(void)
{
    newline_framer_t framer;
    capture_t capture = {0};
    static const uint8_t first[] = "one\r";
    static const uint8_t second[] = "\ntwo\n\n";

    newline_framer_initialize(&framer);
    newline_framer_consume(&framer, first, sizeof(first) - 1U,
                           capture_line, &capture);
    assert(capture.count == 0U);
    newline_framer_consume(&framer, second, sizeof(second) - 1U,
                           capture_line, &capture);

    assert(capture.count == 3U);
    assert(capture.lengths[0] == 3U);
    assert(memcmp(capture.lines[0], "one", 3U) == 0);
    assert(capture.lengths[1] == 3U);
    assert(memcmp(capture.lines[1], "two", 3U) == 0);
    assert(capture.lengths[2] == 0U);
}

static void maximum_line_is_accepted(void)
{
    newline_framer_t framer;
    capture_t capture = {0};
    uint8_t input[NEWLINE_FRAMER_MAX_LINE_LENGTH + 1U];

    memset(input, 'x', sizeof(input));
    input[sizeof(input) - 1U] = '\n';
    newline_framer_initialize(&framer);
    newline_framer_consume(&framer, input, sizeof(input), capture_line,
                           &capture);

    assert(capture.count == 1U);
    assert(capture.lengths[0] == NEWLINE_FRAMER_MAX_LINE_LENGTH);
    assert(framer.overflow_count == 0U);
}

static void oversized_line_is_discarded_and_next_line_recovers(void)
{
    newline_framer_t framer;
    capture_t capture = {0};
    uint8_t input[NEWLINE_FRAMER_MAX_LINE_LENGTH + 6U];

    memset(input, 'x', NEWLINE_FRAMER_MAX_LINE_LENGTH + 1U);
    memcpy(&input[NEWLINE_FRAMER_MAX_LINE_LENGTH + 1U], "\nok\n", 4U);
    newline_framer_initialize(&framer);
    newline_framer_consume(&framer,
                           input,
                           NEWLINE_FRAMER_MAX_LINE_LENGTH + 5U,
                           capture_line,
                           &capture);

    assert(framer.overflow_count == 1U);
    assert(capture.count == 1U);
    assert(capture.lengths[0] == 2U);
    assert(memcmp(capture.lines[0], "ok", 2U) == 0);
}

static void discard_and_counter_saturation_are_safe(void)
{
    newline_framer_t framer;
    capture_t capture = {0};
    static const uint8_t input[] = "ignored\nok\n";

    newline_framer_initialize(&framer);
    framer.overflow_count = UINT32_MAX;
    newline_framer_discard_current_line(&framer);
    newline_framer_consume(&framer, input, sizeof(input) - 1U,
                           capture_line, &capture);

    assert(framer.overflow_count == UINT32_MAX);
    assert(capture.count == 1U);
    assert(capture.lengths[0] == 2U);
}

int main(void)
{
    partial_crlf_and_multiple_lines_are_framed();
    maximum_line_is_accepted();
    oversized_line_is_discarded_and_next_line_recovers();
    discard_and_counter_saturation_are_safe();
    return 0;
}
