/*
 * boot_usb_ch32l103.c
 *
 * CH32L103 USBFS device-controller port glue, register level:
 *   - the CherryUSB low-level hooks (usb_dc_low_level_init / _deinit)
 *   - the teardown used before jumping to the bootloader
 *
 * Compared with the CH32V30x USBHS port the differences are the ones the
 * silicon dictates: USBFS instead of USBHS, and its 48 MHz kernel clock comes
 * from the system PLL (CFGR0.USBPRE) instead of a dedicated PHY PLL in CFGR2.
 * With SYSCLK = 96 MHz that divider is PLL / 2.
 */
#include "boot_usb_port.h"

#include "ch32fun.h"

void usb_dc_low_level_init(uint8_t busid)
{
    (void)busid;

    /* 48 MHz kernel clock: SYSCLK is 96 MHz on this board, so PLL / 2. */
    RCC->CFGR0 = (RCC->CFGR0 & ~(3UL << 22)) | RCC_USBCLKSource_PLLCLK_Div2;

    RCC->AHBPCENR |= RCC_AHBPeriph_USBFS;

    NVIC_EnableIRQ(USBFS_IRQn);
}

void usb_dc_low_level_deinit(uint8_t busid)
{
    (void)busid;
    NVIC_DisableIRQ(USBFS_IRQn);
}

void boot_usb_port_deinit(void)
{
    NVIC_DisableIRQ(USBFS_IRQn);

    /* Clearing BASE_CTRL drops the D+ pull-up, so the host sees a disconnect and
     * re-enumerates the bootloader instead of keeping this device instance. */
    USBFSD->BASE_CTRL = 0;
    RCC->AHBPCENR &= ~RCC_AHBPeriph_USBFS;

    /* >10 ms so the host definitely samples the disconnect. */
    for (volatile uint32_t settle = 0U; settle < 200000U; ++settle) {
    }
}
