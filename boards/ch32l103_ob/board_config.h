/*
 * board_config.h - "ch32l103_ob"
 *
 * Board Support Package for the CH32L103K8U6 that sits on the HPM5151 EVK as
 * its onboard debug probe.  The part runs the probe firmware, drives the
 * target's SWD/JTAG header and exposes a CDC ACM console over USB.  The QFN32
 * package brings out neither OSC_IN/OSC_OUT nor NRST, so the clock tree in
 * board.cmake runs from the internal oscillator.
 *
 * Wiring, from the HPM5151 EVK schematic (L103 debugger section) and the
 * working probe build under /mnt/s/ch32_bmp:
 *
 *   USB         : D+ = PA11 (UDP), D- = PA12 (UDM) - dedicated USBFS pins
 *   RGB LED     : PB8 (red), PB6 (blue), PB7 (green), series resistor to
 *                 VDD_3V3, active low.  This board layer drives the red one as
 *                 the status LED; the other two are documented for later use.
 *   user button : PA0 (WKUP) to GND, internal pull-up, active low
 *   target JTAG : TCK = PA6, TMS/SWDIO = PA7, TDI = PA5, TDO = PA4
 *   console     : USART1, PA9 (TX) / PA10 (RX)
 *
 * Each board lives in its own, self-contained folder under boards/:
 *     boards/<board>/board_config.h   pins / polarity / features (this file)
 *     boards/<board>/board.h          board primitive interface
 *     boards/<board>/board.c          board primitive implementation
 *     boards/<board>/board.cmake      clock tree, chip port, linker script
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#define BOARD_NAME              "ch32l103_ob"

/* ---- BOOT button: PA0 to GND, pull-up, active low ----
 * Held at power-up the bootloader stays in DFU; held while the probe runs, the
 * application requests DFU mode and resets. */
#define BOARD_HAS_BOOT_BUTTON   1
#define BOARD_BOOT_GPIO         GPIOA
#define BOARD_BOOT_PIN          GPIO_Pin_0
#define BOARD_BOOT_RCC          RCC_APB2Periph_GPIOA
#define BOARD_BOOT_ACTIVE_LOW   1

/* ---- status LED: PB8, the red one of the RGB trio ----
 * BOARD_LED_ACTIVE_LOW: 1 = LED on when the pin is low.  PB8 is a plain GPIO,
 * so no JTAG remap is needed before using it. */
#define BOARD_HAS_LED           1
#define BOARD_LED_GPIO          GPIOB
#define BOARD_LED_PIN           GPIO_Pin_8
#define BOARD_LED_RCC           RCC_APB2Periph_GPIOB
#define BOARD_LED_ACTIVE_LOW    1
#define BOARD_LED_NEEDS_JTAG_DISABLE 0

/* ---- application UART: USART1 on the pin header ----
 * This is the probe's own console/aux UART; the target's UART is tunnelled over
 * the second CDC ACM interface and uses no pins on this side. */
#define BOARD_HAS_APP_UART        1
#define BOARD_APP_UART            USART1
#define BOARD_APP_UART_IRQ        USART1_IRQn
#define BOARD_APP_UART_BAUDRATE   115200
#define BOARD_APP_UART_TX_GPIO    GPIOA
#define BOARD_APP_UART_TX_PIN     GPIO_Pin_9
#define BOARD_APP_UART_RX_GPIO    GPIOA
#define BOARD_APP_UART_RX_PIN     GPIO_Pin_10
#define BOARD_APP_UART_CLK_ENABLE()      (RCC->APB2PCENR |= RCC_APB2Periph_USART1)
#define BOARD_APP_UART_GPIO_CLK_ENABLE() (RCC->APB2PCENR |= RCC_APB2Periph_GPIOA)

/* ---- target SWD/JTAG pins ----
 * Read by bmp_port/jtag_port.h.  All four live on GPIOA, which is what the fast
 * combined clock-edge helpers there rely on: one BSHR store moves TCK, TMS and
 * TDI together. */
#define BOARD_JTAG_TCK_GPIO     GPIOA
#define BOARD_JTAG_TCK_PIN      GPIO_Pin_6
#define BOARD_JTAG_TMS_GPIO     GPIOA
#define BOARD_JTAG_TMS_PIN      GPIO_Pin_7
#define BOARD_JTAG_TDI_GPIO     GPIOA
#define BOARD_JTAG_TDI_PIN      GPIO_Pin_5
#define BOARD_JTAG_TDO_GPIO     GPIOA
#define BOARD_JTAG_TDO_PIN      GPIO_Pin_4
#define BOARD_JTAG_GPIO_RCC     RCC_APB2Periph_GPIOA

#endif /* BOARD_CONFIG_H */
