# bmp-ch32xx-port

[English](README.md) | [简体中文](README.zh-CN.md)

Black Magic Probe (BMP) ported to the CH32xx family: **CH32V30x** (high-speed USBHS)
and **CH32L103** (full-speed USBFS). Both are built as the **application image** of a
DFU bootloader: the bootloader owns the first 12 KB of flash and the probe starts at
`0x08003000`.

```
CMakeLists.txt            top level: toolchain, board selection, link, objcopy
main.c                    application entry point
CMakePresets.json         per-board release / debug presets
bmp_port/                 port layer: USB, bit-banged SWD/JTAG, target UART,
                          timing, bootloader handshake, MCU glue
bmp_port/CMakeLists.txt   port sources + blackmagic sources and includes
cmake/ch32fun_riscv.cmake toolchain file (riscv-wch-elf-)
third_party_components/
  blackmagic/             submodule: BMP core and target drivers
  CherryUSB/              submodule: USB stack + CH32V30x USBHS port
  ch32fun/                submodule: startup, clock tree, vectors, registers
boards/ch32v30x_ob/       board BSP: CH32V30x, external crystal, USBHS
boards/ch32l103_ob/       board BSP: CH32L103, HSI only, USBFS
linkfile/flash_dfu.ld           application linker script, 116K (V30x)
linkfile/flash_dfu_ch32l103.ld  application linker script, 52K (L103)
shared/boot_protocol.h    bootloader/application contract (partition, BKP)
docs/                     development notes (problem records, design notes)
```

## Boards

| Board | MCU / clock | USB device | Application region |
|---|---|---|---|
| `ch32v30x_ob` | CH32V305/307, 24 MHz crystal, 144 MHz sysclk | USBHS, high speed | 116 KB at `0x08003000` |
| `ch32l103_ob` | CH32L103K8U6 (onboard probe of the HPM5151 EVK), internal oscillator, 96 MHz sysclk | USBFS, full speed | 52 KB at `0x08003000` |

The chip-specific pieces are selected by `boards/<board>/board.cmake`
(`BOARD_CHIP` = `ch32v30x` / `ch32l103`, `BOARD_USB_HS`, linker script); everything
above that - the composite USB descriptors, the target UART data path, the
bootloader handshake - is shared.

The L103 parts are 64 KB / 20 KB, so 52 KB of application flash is not enough for
the full multi-architecture probe (~88 KB): that board is built with
`-DBMP_SLIM=ON` (RISC-V/HPM targets only, ARM/ADIv5/SPI dropped), which is what the
`ch32l103_ob-slim-*` presets do. The V30x also has `-slim-` presets for the smaller
image.

## Pins

`ch32v30x_ob`:

| Function | Pin |
|---|---|
| SWCLK / TCK | PB14 |
| SWDIO / TMS | PB15 |
| TDI | PB13 |
| TDO | PB12 |
| Status LED (active low) | PA5 |
| BOOT button (to GND, active low) | PA6 |
| Target UART TX (USART3) | PB10 |
| Target UART RX (USART3) | PB11 |

`ch32l103_ob`:

| Function | Pin |
|---|---|
| SWCLK / TCK | PA6 |
| SWDIO / TMS | PA7 |
| TDI | PA5 |
| TDO | PA4 |
| Status LED (red of the RGB trio, active low) | PB8 |
| BOOT button (to GND, active low) | PA0 |
| Console UART TX (USART1) | PA9 |
| Console UART RX (USART1) | PA10 |

nTRST / nSRST are not wired on either board (empty implementations). The SWD/JTAG
pin definitions live in `bmp_port/jtag_port.h`; the status LED and BOOT button are
board pins (`BOARD_LED_*` / `BOARD_BOOT_*` in `boards/<board>/board_config.h`)
reached through the board primitives `board_led_write()` / `board_read_boot_pin()`.

SWD/JTAG is **GPIO bit-banging only** (the `PIN_*` macros in
`bmp_port/jtag_port.h`); there is no SPI acceleration path, so the timing stays
fully deterministic.

## USB device (composite, 1a86:6018)

Same descriptors on both boards; only the underlying controller differs (USBHS on
the V30x, USBFS on the L103).

| Interface | Function |
|---|---|
| 0 / 1 | GDB server (CDC ACM) |
| 2 / 3 | target UART (CDC ACM) |
| 4 | DFU runtime (no endpoints) |

- **The DFU runtime interface (4) announces itself through Microsoft OS 1.0
  descriptors (WCID)**: the 0xEE string plus `bRequest = 0x20`, Compatible ID
  `WINUSB`, and the `DeviceInterfaceGUIDs` extended property (required - CherryUSB
  dereferences `comp_id_property` for `wIndex = 5`). Windows then **binds WinUSB
  automatically**, so `dfu-util -e` works out of the box and no Zadig is needed.
- Windows caches the WCID result per `VID/PID/bcdDevice` in
  `HKLM\SYSTEM\CurrentControlSet\Control\usbflags`, so `bcdDevice` (or the VID/PID)
  has to change to make it ask again.
- The two CDC functions (GDB and target UART) are **deliberately not listed** in
  the WCID data, so Windows keeps using the inbox usbser.sys driver and they stay
  COM ports.
- The DFU interface `iInterface` points at string 6,
  `"Black Magic Firmware Upgrade"`, so `dfu-util -l` shows a proper `name=`
  instead of `UNKNOWN`.
- VID is WCH's `0x1A86` (same vendor as the matching DFU bootloader's
  `1A86:DF11`); the PID stays Black Magic's `0x6018`.
- `bcdUSB` = 2.0, `bcdDevice` = `0x0104`. `bcdDevice` is part of the Windows
  hardware ID and of the usbflags cache key, so bump it whenever the descriptors
  or the WCID data change, to force Windows to re-enumerate and reinstall.
- **No RTT**: this port exposes the GDB port and the target UART only.

## Target UART (USB2UART)

The second CDC function (interfaces 2/3) is wired to the board's application UART:
USART3 (PB10/PB11) on the V30x, USART1 (PA9/PA10) on the L103. The UART *hardware*
is described by the board, not the port: instance, pins and clocks are the
`BOARD_APP_UART*` macros in `boards/<board>/board_config.h`, brought up by
`board_init_app_uart()`. The port owns the line format and everything above the
wire. The whole path is **DMA + interrupt driven** and never depends on the main
loop:

- **target → host**: the UART RX stream is written into a 256 byte buffer in a
  circle by DMA; the UART **idle interrupt** and the DMA half/full transfer
  interrupts hand whatever arrived straight to the IN endpoint through a USB sink,
  and the endpoint's completion callback (`aux_serial_usb_ready()`) continues with
  the rest.
- **host → target**: the USB OUT callback queues into a 1 KB ring and DMA sends one
  contiguous run at a time, its transfer complete interrupt picking up the next one.
- **No DTR gating**: data is forwarded as long as the host is reading, so terminals
  that never assert DTR work too.
- The UART therefore keeps flowing while a long GDB command is in progress; the old
  implementation forwarded it from the `gdb_if_getchar()` idle loop, which stalled.

## Bootloader integration

- The application links at `0x00003000` (`linkfile/flash_dfu.ld`, 116 KB for the
  V30x; `linkfile/flash_dfu_ch32l103.ld`, 52 KB for the L103), matching
  `BOOT_APP_OFFSET` in `shared/boot_protocol.h`.
- Three paths return to the bootloader, all going through the BKP handshake in
  `bmp_port/boot_trigger_<chip>.c` (it survives `NVIC_SystemReset()`):
  - `dfu-util -e` (DFU_DETACH) calls `platform_request_boot()`
  - `monitor bootloader` from the GDB side
  - holding the BOOT button: the probe registers a 100 ms tick through
    `board_timer_create()` (`bmp_port/platform.c`), which a board timer interrupt
    calls, so it works while the main loop is parked in `gdb_if_getchar()` waiting
    for a GDB command

## Build

```sh
# CH32V30x (full image)
cmake --preset ch32v30x_ob-release
cmake --build --preset ch32v30x_ob-release

# CH32L103 (slim image, required for the 52 KB partition)
cmake --preset ch32l103_ob-slim-release
cmake --build --preset ch32l103_ob-slim-release
```

Artifacts are named after the board, so a build directory never mixes them up:

- `build/ch32v30x_ob-release/bmp-ch32v30x_ob.{elf,hex,bin}`
- `build/ch32l103_ob-slim-release/bmp-ch32l103_ob.{elf,hex,bin}`

## Flashing / usage

1. Flash a matching DFU bootloader first (it owns its own 12 KB region at
   `0x08000000`).
2. Write this application. Because it exposes a **DFU runtime interface** (interface 4),
   the **same single command works in both states** - you never have to find out which
   firmware the device is running first:

   ```sh
   # CH32V30x
   dfu-util -d 1a86 -s 0x08003000:leave -D build/ch32v30x_ob-release/bmp-ch32v30x_ob.bin
   # CH32L103
   dfu-util -d 1a86 -s 0x08003000:leave -D build/ch32l103_ob-slim-release/bmp-ch32l103_ob.bin
   ```

   - **Device already in the bootloader's DFU mode** (entered by holding BOOT while
     powering up) - plain download.
   - **Device running this application** (`1a86:6018`) - `dfu-util` sends DFU_DETACH
     before the download; the runtime interface hands it to `platform_request_boot()`,
     which writes the BKP hand-shake and resets, so the bootloader comes up in DFU mode
     and the command flashes the image.
   - `-d 1a86` matches by VID only (`dfu-util` also matches DFU-mode devices when only
     run-time IDs are given), which covers the application (`1a86:6018`) and the
     bootloader (`1a86:df11`) alike, so the switch of mode does not lose the device.
   - To only send a running application back to the bootloader without flashing, use
     `dfu-util -e` on its own.

3. Then connect with GDB:
   ```sh
   arm-none-eabi-gdb -ex 'target extended-remote /dev/ttyACM0'   # Linux
   # Windows: COMx / WinUSB, depending on your GDB build
   ```
4. To go back to the bootloader: `dfu-util -e` (or `monitor bootloader` in GDB).

## Development notes

Problem records and design notes live under `docs/`, each in English and Chinese.
This README is the engineering overview only; the reasoning lives in the notes:

- [`docs/gdb-reply-path.md`](docs/gdb-reply-path.md) - the CH32V30x USBHS IN
  transfer problem behind the long GDB reply stalls, the reply-size limit that
  keeps every reply inside one USB packet (`GDB_PACKET_BUFFER_SIZE = 496`), and the
  residual risks.
- [`docs/jtag-clock-duty-cycle.md`](docs/jtag-clock-duty-cycle.md) - why the JTAG
  clocks that measured ~86% duty cycle did so (the tail bits of every bulk shift,
  and the single-clock paths), and how the shift loops are arranged for 50%.

## Notes

- Submodules, both under `third_party_components/`: `blackmagic`
  (`https://codeberg.org/mTOTm/blackmagic.git`, `dev/jiance.zhang/main_test`) and
  `CherryUSB` (`ch32v30x-usbhs`, containing the CH32V30x USBHS device controller
  port; the L103 uses CherryUSB's own `port/wch/usbfs`).
- The target driver list is trimmed in `bmp_port/CMakeLists.txt` to fit the
  application partition (116 KB on the V30x, 52 KB on the L103); add files there to
  support more chips.
