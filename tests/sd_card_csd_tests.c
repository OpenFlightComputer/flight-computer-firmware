#include "sd_card_csd.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

int main(void)
{
    uint64_t sector_count = 0U;
    uint8_t version_two[16] = {0};
    uint8_t version_one[16] = {0};
    uint8_t reserved[16] = {0};

    version_two[0] = 0x40U;
    assert(sd_card_parse_csd(version_two, &sector_count));
    assert(sector_count == 1024U);

    version_one[5] = 9U;
    assert(sd_card_parse_csd(version_one, &sector_count));
    assert(sector_count == 4U);

    reserved[0] = 0x80U;
    assert(!sd_card_parse_csd(reserved, &sector_count));
    assert(!sd_card_parse_csd(NULL, &sector_count));
    assert(!sd_card_parse_csd(version_two, NULL));
    return 0;
}
