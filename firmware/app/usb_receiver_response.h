#ifndef OPENFLIGHTCOMPUTER_USB_RECEIVER_RESPONSE_H
#define OPENFLIGHTCOMPUTER_USB_RECEIVER_RESPONSE_H

#include "receiver_inspection.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool usb_receiver_response_build(const receiver_inspection_t *inspection,
                                 uint32_t request_id,
                                 char *destination,
                                 size_t capacity,
                                 size_t *length);

#endif
