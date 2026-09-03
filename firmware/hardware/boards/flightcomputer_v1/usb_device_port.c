#include "usbd_core.h"
#include "usbd_cdc.h"

#include "board_definition.h"
#include "stm32f4xx_hal.h"

#include <stddef.h>
#include <stdint.h>

static PCD_HandleTypeDef openflightcomputer_usb_pcd;

_Static_assert(
    (FLIGHTCOMPUTER_V1_USB_VBUS_MODE == BOARD_USB_VBUS_MODE_ASSUME_PRESENT) ||
        (FLIGHTCOMPUTER_V1_USB_VBUS_MODE == BOARD_USB_VBUS_MODE_SENSE_INPUT),
    "Flight Computer V1 must select a supported USB VBUS mode");

static uint32_t usb_class_storage[
    (sizeof(USBD_CDC_HandleTypeDef) + sizeof(uint32_t) - 1U) / sizeof(uint32_t)
];

static USBD_StatusTypeDef usbd_status(HAL_StatusTypeDef status)
{
    return status == HAL_OK ? USBD_OK : USBD_FAIL;
}

void OTG_FS_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&openflightcomputer_usb_pcd);
}

void *USBD_static_malloc(uint32_t size)
{
    if (size > sizeof(usb_class_storage)) {
        return NULL;
    }

    return usb_class_storage;
}

void USBD_static_free(void *pointer)
{
    (void)pointer;
}

void HAL_PCD_MspInit(PCD_HandleTypeDef *pcd)
{
    GPIO_InitTypeDef gpio = {0};

    if (pcd->Instance != USB_OTG_FS) {
        return;
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF10_OTG_FS;
    HAL_GPIO_Init(GPIOA, &gpio);

    if (board_usb_vbus_sensing_enabled(FLIGHTCOMPUTER_V1_USB_VBUS_MODE)) {
        gpio.Pin = GPIO_PIN_9;
        gpio.Mode = GPIO_MODE_INPUT;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        gpio.Alternate = 0U;
        HAL_GPIO_Init(GPIOA, &gpio);
    }

    __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

    HAL_NVIC_SetPriority(OTG_FS_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *pcd)
{
    if (pcd->Instance != USB_OTG_FS) {
        return;
    }

    HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
    if (board_usb_vbus_sensing_enabled(FLIGHTCOMPUTER_V1_USB_VBUS_MODE)) {
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9);
    }
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *pcd)
{
    USBD_LL_SetupStage(pcd->pData, (uint8_t *)pcd->Setup);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *pcd, uint8_t endpoint)
{
    USBD_LL_DataOutStage(
        pcd->pData,
        endpoint,
        pcd->OUT_ep[endpoint].xfer_buff
    );
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *pcd, uint8_t endpoint)
{
    USBD_LL_DataInStage(
        pcd->pData,
        endpoint,
        pcd->IN_ep[endpoint].xfer_buff
    );
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *pcd)
{
    USBD_LL_SOF(pcd->pData);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *pcd)
{
    USBD_LL_SetSpeed(pcd->pData, USBD_SPEED_FULL);
    USBD_LL_Reset(pcd->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *pcd)
{
    USBD_LL_Suspend(pcd->pData);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *pcd)
{
    USBD_LL_Resume(pcd->pData);
}

void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *pcd, uint8_t endpoint)
{
    USBD_LL_IsoOUTIncomplete(pcd->pData, endpoint);
}

void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *pcd, uint8_t endpoint)
{
    USBD_LL_IsoINIncomplete(pcd->pData, endpoint);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *pcd)
{
    USBD_LL_DevConnected(pcd->pData);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *pcd)
{
    USBD_LL_DevDisconnected(pcd->pData);
}

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *device)
{
    openflightcomputer_usb_pcd.Instance = USB_OTG_FS;
    openflightcomputer_usb_pcd.Init.dev_endpoints = 4U;
    openflightcomputer_usb_pcd.Init.use_dedicated_ep1 = 0U;
    openflightcomputer_usb_pcd.Init.dma_enable = 0U;
    openflightcomputer_usb_pcd.Init.low_power_enable = 0U;
    openflightcomputer_usb_pcd.Init.phy_itface = PCD_PHY_EMBEDDED;
    openflightcomputer_usb_pcd.Init.Sof_enable = 0U;
    openflightcomputer_usb_pcd.Init.speed = PCD_SPEED_FULL;
    openflightcomputer_usb_pcd.Init.vbus_sensing_enable =
        board_usb_vbus_sensing_enabled(FLIGHTCOMPUTER_V1_USB_VBUS_MODE) ? 1U
                                                                       : 0U;

    openflightcomputer_usb_pcd.pData = device;
    device->pData = &openflightcomputer_usb_pcd;

    if (HAL_PCD_Init(&openflightcomputer_usb_pcd) != HAL_OK) {
        return USBD_FAIL;
    }

    if (HAL_PCDEx_SetRxFiFo(&openflightcomputer_usb_pcd, 0x80U) != HAL_OK ||
        HAL_PCDEx_SetTxFiFo(&openflightcomputer_usb_pcd, 0U, 0x40U) != HAL_OK ||
        HAL_PCDEx_SetTxFiFo(&openflightcomputer_usb_pcd, 1U, 0x60U) != HAL_OK ||
        HAL_PCDEx_SetTxFiFo(&openflightcomputer_usb_pcd, 2U, 0x20U) != HAL_OK) {
        return USBD_FAIL;
    }

    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *device)
{
    return usbd_status(HAL_PCD_DeInit(device->pData));
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *device)
{
    return usbd_status(HAL_PCD_Start(device->pData));
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *device)
{
    return usbd_status(HAL_PCD_Stop(device->pData));
}

USBD_StatusTypeDef USBD_LL_OpenEP(
    USBD_HandleTypeDef *device,
    uint8_t endpoint,
    uint8_t type,
    uint16_t maximum_packet_size
)
{
    return usbd_status(HAL_PCD_EP_Open(
        device->pData,
        endpoint,
        maximum_packet_size,
        type
    ));
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *device, uint8_t endpoint)
{
    return usbd_status(HAL_PCD_EP_Close(device->pData, endpoint));
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *device, uint8_t endpoint)
{
    return usbd_status(HAL_PCD_EP_Flush(device->pData, endpoint));
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *device, uint8_t endpoint)
{
    return usbd_status(HAL_PCD_EP_SetStall(device->pData, endpoint));
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(
    USBD_HandleTypeDef *device,
    uint8_t endpoint
)
{
    return usbd_status(HAL_PCD_EP_ClrStall(device->pData, endpoint));
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *device, uint8_t endpoint)
{
    PCD_HandleTypeDef *pcd = device->pData;

    if ((endpoint & 0x80U) != 0U) {
        return pcd->IN_ep[endpoint & 0x7FU].is_stall;
    }

    return pcd->OUT_ep[endpoint & 0x7FU].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(
    USBD_HandleTypeDef *device,
    uint8_t address
)
{
    return usbd_status(HAL_PCD_SetAddress(device->pData, address));
}

USBD_StatusTypeDef USBD_LL_Transmit(
    USBD_HandleTypeDef *device,
    uint8_t endpoint,
    uint8_t *buffer,
    uint32_t size
)
{
    return usbd_status(HAL_PCD_EP_Transmit(
        device->pData,
        endpoint,
        buffer,
        size
    ));
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(
    USBD_HandleTypeDef *device,
    uint8_t endpoint,
    uint8_t *buffer,
    uint32_t size
)
{
    return usbd_status(HAL_PCD_EP_Receive(
        device->pData,
        endpoint,
        buffer,
        size
    ));
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *device, uint8_t endpoint)
{
    return HAL_PCD_EP_GetRxCount(device->pData, endpoint);
}

void USBD_LL_Delay(uint32_t milliseconds)
{
    HAL_Delay(milliseconds);
}
