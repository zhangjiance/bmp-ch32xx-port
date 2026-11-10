/*
 * ch32l103_it.c
 *
 * Exception handlers for the CH32L103 BMP port.
 *
 * ch32fun's vector table carries weak aliases for every handler, so these strong
 * definitions replace the defaults.  The USBFS and USART1 vectors are served by
 * their drivers (the CherryUSB usbfs port and aux_serial.c).
 */
#include "ch32fun.h"

void NMI_Handler(void) INTERRUPT_DECORATOR;
void HardFault_Handler(void) INTERRUPT_DECORATOR;

void NMI_Handler(void)
{
    while (1) {
    }
}

void HardFault_Handler(void)
{
    NVIC_SystemReset();
    while (1) {
    }
}
