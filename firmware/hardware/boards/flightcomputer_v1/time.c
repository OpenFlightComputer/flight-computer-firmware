#include "time.h"

#include "mcu_timebase.h"

uint64_t time_us(void)
{
    return mcu_timebase_us();
}
