#ifndef OPENFLIGHTCOMPUTER_DSHOT_ENCODER_H
#define OPENFLIGHTCOMPUTER_DSHOT_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

#define DSHOT_FRAME_BIT_COUNT 16U
#define DSHOT_VALUE_STOP UINT16_C(0)
#define DSHOT_COMMAND_MIN UINT16_C(1)
#define DSHOT_COMMAND_MAX UINT16_C(47)
#define DSHOT_THROTTLE_MIN UINT16_C(48)
#define DSHOT_THROTTLE_MAX UINT16_C(2047)

typedef enum {
    DSHOT_ENCODE_OK = 0,
    DSHOT_ENCODE_INVALID_ARGUMENT,
    DSHOT_ENCODE_INVALID_THROTTLE,
    DSHOT_ENCODE_INVALID_COMMAND,
} dshot_encode_result_t;

/* Accepts only stop (0) or a throttle value in the inclusive range 48..2047. */
dshot_encode_result_t dshot_encode_throttle(uint16_t throttle,
                                            bool request_telemetry,
                                            uint16_t *frame);

/* Accepts only a special command in the inclusive range 1..47. */
dshot_encode_result_t dshot_encode_command(uint16_t command,
                                           bool request_telemetry,
                                           uint16_t *frame);

#endif
