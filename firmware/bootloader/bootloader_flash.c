#include "bootloader_flash.h"

#include "bootloader_contract.h"
#include "stm32f4xx_hal.h"

#include <string.h>

bool bootloader_flash_begin(void)
{
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_SECTORS,
        .Sector = FLASH_SECTOR_4,
        .NbSectors = 7U,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
    };
    uint32_t sector_error = 0U;
    bool ok;

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return false;
    }
    ok = HAL_FLASHEx_Erase(&erase, &sector_error) == HAL_OK;
    if (HAL_FLASH_Lock() != HAL_OK) {
        ok = false;
    }
    return ok;
}

bool bootloader_flash_write(uint32_t offset,
                            const uint8_t *data,
                            size_t length)
{
    uint32_t address;
    size_t position = 0U;
    bool ok = true;

    if ((data == NULL) || (length == 0U) ||
        ((offset % sizeof(uint32_t)) != 0U) ||
        (offset > OFC_APPLICATION_MAXIMUM_LENGTH) ||
        (length > OFC_APPLICATION_MAXIMUM_LENGTH - offset)) {
        return false;
    }
    address = OFC_APPLICATION_FLASH_START + offset;
    if (HAL_FLASH_Unlock() != HAL_OK) {
        return false;
    }
    while ((position < length) && ok) {
        uint32_t word = UINT32_MAX;
        const size_t remaining = length - position;
        const size_t copied = remaining < sizeof(word) ? remaining : sizeof(word);

        memcpy(&word, &data[position], copied);
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, word) == HAL_OK;
        address += sizeof(word);
        position += copied;
    }
    if (HAL_FLASH_Lock() != HAL_OK) {
        ok = false;
    }
    return ok;
}

static bool program_metadata_word(uint32_t offset, uint32_t value)
{
    return HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                             OFC_APPLICATION_METADATA_ADDRESS + offset,
                             value) == HAL_OK;
}

bool bootloader_flash_commit(uint32_t image_length, uint32_t image_crc32)
{
    bool ok;

    if ((image_length < 8U) ||
        (image_length > OFC_APPLICATION_MAXIMUM_LENGTH) ||
        (HAL_FLASH_Unlock() != HAL_OK)) {
        return false;
    }
    ok = program_metadata_word(4U, OFC_APPLICATION_METADATA_VERSION) &&
         program_metadata_word(8U, image_length) &&
         program_metadata_word(12U, image_crc32) &&
         program_metadata_word(0U, OFC_APPLICATION_METADATA_MAGIC);
    if (HAL_FLASH_Lock() != HAL_OK) {
        ok = false;
    }
    return ok;
}
