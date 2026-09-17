#include "bootloader_crc32.h"

#include <assert.h>
#include <stdint.h>

int main(void)
{
    static const uint8_t reference[] = "123456789";

    assert(bootloader_crc32(reference, 0U) == UINT32_C(0x00000000));
    assert(bootloader_crc32(reference, sizeof(reference) - 1U) ==
           UINT32_C(0xCBF43926));
    return 0;
}
