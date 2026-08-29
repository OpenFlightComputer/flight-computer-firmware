#ifndef OPENFLIGHTCOMPUTER_USB_HEALTH_RESPONSE_H
#define OPENFLIGHTCOMPUTER_USB_HEALTH_RESPONSE_H

#include "fault.h"
#include "health.h"

#include <stdbool.h>
#include <stddef.h>

bool usb_health_response_build(const health_summary_t *summary,
                               const fault_system_t *fault_system,
                               char *destination,
                               size_t capacity,
                               size_t *length);

#endif
