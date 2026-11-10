# ==============================================================================
# boards/ch32l103_ob/board.cmake
#
# Board-specific build definitions.  Included by the top level CMakeLists.txt
# when BOARD selects this directory.
# ==============================================================================

# ------------------------------------------------------------------------------
# Clock tree: internal oscillator, no crystal fitted
#
#     HSI 8 MHz  ->  PLL x12  ->  SYSCLK = HCLK = 96 MHz, APB1 = HCLK/2 = 48 MHz
#
# The QFN32 (CH32L103K8U6) pinout brings out neither OSC_IN/OSC_OUT nor NRST, so
# HSI is the only clock source and reset is power-on or software only.  On the
# HSI path ch32fun sets EXTEN_PLL_HSI_PRE, which selects HSI as the PLL input
# without dividing it, so the PLL sees the full 8 MHz; x12 is the 96 MHz
# maximum.  The USBFS cell then takes PLL / 2 = 48 MHz, which is the divider the
# chip port's boot_usb_ch32l103.c programs.
# ------------------------------------------------------------------------------
# CH32L103 selects ch32l103hw.h in ch32fun (funconfig.h then leaves CH32V30x
# undefined); the register map and the vector table follow from it.
add_compile_definitions(
    CH32L103=1
    HSE_VALUE=8000000
    FUNCONF_USE_HSI=1
    FUNCONF_USE_HSE=0
    FUNCONF_PLL_MULTIPLIER=12
    FUNCONF_SYSTEM_CORE_CLOCK=96000000
)

# ------------------------------------------------------------------------------
# Chip port selection
#
# bmp_port/CMakeLists.txt picks the boot_*, *_it.c and CherryUSB controller
# sources from these two variables:
#   BOARD_CHIP    ch32l103 -> USBFS device controller
#   BOARD_USB_HS  OFF      -> full speed, no CONFIG_USB_HS / USB_HS
# ------------------------------------------------------------------------------
set(BOARD_CHIP "ch32l103")
set(BOARD_USB_HS OFF)

# ------------------------------------------------------------------------------
# Linker script
#
# 64 KB part: the 12 KB DFU bootloader owns the bottom, so the probe links at
# 0x00003000 with 52 KB (13 whole DfuSe sectors) and 20 KB of RAM.
# ------------------------------------------------------------------------------
set(BOARD_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linkfile/flash_dfu_ch32l103.ld")

# ------------------------------------------------------------------------------
# Image size
#
# 52 KB of application flash is not enough for the full multi-architecture probe
# (that build is ~88 KB); this board has to be built with -DBMP_SLIM=ON, which
# keeps the RISC-V/HPM target support and drops ARM/ADIv5/SPI.  The
# ch32l103_ob-slim-* presets do that.
# ------------------------------------------------------------------------------
