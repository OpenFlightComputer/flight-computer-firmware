#ifndef OPENFLIGHTCOMPUTER_BOOTLOADER_PROTOCOL_H
#define OPENFLIGHTCOMPUTER_BOOTLOADER_PROTOCOL_H

#include <stdbool.h>

void bootloader_protocol_initialize(bool application_valid,
                                    bool vbus_sensing_enabled);
void bootloader_protocol_process(void);

#endif
