#ifndef OPENFLIGHTCOMPUTER_BOOTLOADER_IMAGE_H
#define OPENFLIGHTCOMPUTER_BOOTLOADER_IMAGE_H

#include <stdbool.h>

bool bootloader_application_is_valid(void);
_Noreturn void bootloader_start_application(void);

#endif
