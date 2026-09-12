#ifndef USB_CONTROL_TRACE_RESPONSE_H
#define USB_CONTROL_TRACE_RESPONSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "control_trace.h"
#include "system_state.h"

bool usb_control_trace_status_response_build(const char *command,
                                             uint32_t request_id,
                                             bool accepted,
                                             system_state_t system_state,
                                             const char *error,
                                             const control_trace_t *trace,
                                             char *response,
                                             size_t response_capacity,
                                             size_t *response_length);

bool usb_control_trace_read_response_build(uint32_t request_id,
                                           const control_trace_t *trace,
                                           const control_trace_record_t *records,
                                           const control_trace_batch_t *batch,
                                           char *response,
                                           size_t response_capacity,
                                           size_t *response_length,
                                           size_t *serialized_record_count);

#endif
