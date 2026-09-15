#ifndef OPENFLIGHTCOMPUTER_USB_BLACKBOX_RESPONSE_H
#define OPENFLIGHTCOMPUTER_USB_BLACKBOX_RESPONSE_H

#include "blackbox.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool usb_blackbox_status_response_build(uint32_t request_id,
                                        const blackbox_t *blackbox,
                                        char *destination,
                                        size_t capacity,
                                        size_t *length);
bool usb_blackbox_initialize_response_build(uint32_t request_id,
                                            bool accepted,
                                            const blackbox_t *blackbox,
                                            const char *error,
                                            char *destination,
                                            size_t capacity,
                                            size_t *length);
bool usb_blackbox_log_list_response_build(uint32_t request_id,
                                          const blackbox_t *blackbox,
                                          char *destination,
                                          size_t capacity,
                                          size_t *length);
bool usb_blackbox_log_read_response_build(
    uint32_t request_id,
    uint32_t log_id,
    uint32_t sector_offset,
    const uint8_t sector[SD_CARD_SECTOR_SIZE],
    char *destination,
    size_t capacity,
    size_t *length);

#endif
