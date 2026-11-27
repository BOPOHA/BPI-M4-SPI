# Manual SPI updates and recovery

Installing the SPI RPM places firmware files under /usr/lib/bpi-m4/spi-2GB/
(or spi-1GB/); it never flashes hardware. The modern loader has no SPI write
command and the current Linux kernel has no usable SPI-NOR/MTD updater.

The raw out/spi/spi-boot-region.bin is not a complete chip image. A complete
image must preserve the actual board's ROM prefix, parameters and factory tail
from a verified full-chip backup. Keep private dumps outside checkout cleanup
paths. Do not apply a different board's backup or a fixed historical write span
to a new firmware size.

The tested UART procedure is retained in the frozen historical repository:
[backup, bounded write, full-chip readback and restoration](https://github.com/BOPOHA/BPI-M4-fc/blob/ab5375ceb78cdb83a8fc3d87257f866a81a5d35d/docs/SPI-UPDATE.md).
It documents a specific 2 GiB board and artifact; it is evidence for recovery,
not a universal installer. The operator performs all UART/privileged operations
manually. Review the exact board, baseline, layout and write range before use.

Before power-off after a write, verify full-chip readback against the intended
image. Preserve a known working restore image and recovery route. Early USB
SPI recovery remains unqualified; see [VALIDATION.md](VALIDATION.md).
After an approved update, cold boot and verify loader identity, CPU count,
installed RAM, SD root, boot-bundle integrity and networking. Media qualification
is separate from a successful Server boot. No update is performed by build tools.
