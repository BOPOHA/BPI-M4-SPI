# SW2=1 early USB rescue observation

On 2026-11-27 the operator selected SW2=1 on the original 2 GiB BPI-M4 and
reported this repeating UART output:

```text
C1:80000000
C2
?_?
C3hUDA get hwsetting fail: BOOT1 Rescue...
Fail to get hwsetting at BOOT1 rescue mode(0x00000015)
00000015
C1:80000000
C2
?_?uu3-1
```

Connecting the board's USB port to the laptop produced:

```text
idVendor=0bda, idProduct=1195, bcdDevice=1.00
Product: USB REDIRECTION
Manufacturer: Realtek
SerialNumber: 0357802115946
```

UART then repeated `[GO]` and `ST`, with one reported `[SET_C]` marker.
No U-Boot banner or Linux boot was reported in this switch position.

## Read-only verification

Regular-user host checks confirmed `0bda:1195` as `Realtek Semiconductor Corp.
USB REDIRECTION`. Cached descriptors showed USB 2.0, a 480 Mbit/s connection,
one vendor-specific interface (class `0xff`, subclass `0xff`, protocol `0x50`)
and two bulk endpoints: IN `0x81` and OUT `0x02`, each with a 512-byte maximum
packet size. The host user could not open the device, so these checks used
cached descriptors; no recovery commands or payloads were sent.

This interface is not USB mass storage. Enumeration establishes that an early
USB interface is alive, but a host downloader must implement its protocol.
It does not establish successful RAM execution or SPI access.

The vendor ARM early-boot binary contains the exact `UDA get hwsetting fail:
BOOT1 Rescue...`, `Fail to get hwsetting at BOOT1 rescue mode(0x`, `[SET_C]`
and `[GO]` strings. The inspected file is
`examples/flash_writer_vm/bootimage/rtd1395/0001/bootmon-new.bin` in the BSP
pinned at `25f5b88ec4ba34029f964693dc34028b26e6c67c`; its SHA256 is
`edda59e18466e8d23f879c42973c7f16b63cd80940acea655314e991e94757c5`.
These messages therefore belong to vendor early firmware, before the modern
U-Boot and Linux. Their presence in this binary does not establish that every
message originates in immutable mask ROM. The exact meanings of `[GO]` and
`ST` have not been decoded; they are not evidence of a successful flash.

The same SW2=1 hardware-settings failure was recorded before the modern SPI
replacement in `../BPI-M4-PoC/docs/HARDWARE.md`, alongside the original vendor
SPI boot at SW2=0 (capture `captures/uart-20261116T132544600974Z.bin`). This
observation is separate from the Linux eMMC driver's inherited-clock probe
failure recorded in [the common Server boot](common-server-2026-11-27.md).

## Proposed Windows recovery method and its verification status

The operator supplied a possible recovery method: use Realtek RtUsbIsp/MPTool
on Windows with a board-specific factory package, select SW2=1, hold an
Install/U-Boot button while connecting USB Type-C, release it after detection,
and flash the factory image before returning SW2 to 0. This is a research lead,
not a tested procedure for this board or the saved SPI backup.

The [official BPI-M4 getting-started page](https://docs.banana-pi.org/en/BPI-M4/GettingStarted_BPI-M4),
checked on 2026-11-27, confirms SW2=1 and a Windows-only BPI-M4 USB flash tool.
Its description is explicitly “flashing android image to board emmc”; it
directs readers to `document/bananapi_m4_usb_download_tool_guide.doc` inside
[the tool package](https://download.banana-pi.dev/d/3ebbfa04265d4dddb81b/files/?p=%2FTools%2Fimage_download_tools%2Fm4_android_usb_download_tool.zip).
The page does not confirm the proposed button sequence, the generic tool
names, or that the factory package restores SPI. The package and its guide
have not been inspected in this observation.

Further recovery qualification belongs to `BPI-M4-SPI`: inspect the official
guide and package, identify the boot/download protocol and RAM-size-specific
payloads, establish which storage regions the tool writes, and verify that a
saved SPI image can actually be restored when U-Boot cannot start. USB device
detection alone does not qualify this restore path. No SPI or eMMC write was
performed for this investigation.

For normal Fedora operation, power the board off, select SW2=0, and power it
on again. The demonstrated UART SPI update procedure remains documented in
[SPI-UPDATE.md](../SPI-UPDATE.md).
