#ifndef OPENFLIGHTCOMPUTER_SYSTEM_CLOCK_H
#define OPENFLIGHTCOMPUTER_SYSTEM_CLOCK_H

#include "stm32f4xx_hal.h"

#define SYSTEM_CLOCK_FREQUENCY_HZ 168000000U

HAL_StatusTypeDef system_clock_configure(void);

#endif
