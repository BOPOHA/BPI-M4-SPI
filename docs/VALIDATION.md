# Validation and release status

The independent SPI build passed U-Boot compilation, source/configuration checks,
packer/crypto comparisons, DDR/FSBL checks, layout and secondary-entry disassembly.
All four controller fault regressions passed in regular-user Mock. Exact source,
artifact and tool identities are in [validation.json](validation.json).

Historical hardware results describe earlier installed artifacts, not a rebuilt
release candidate:

- [CPU-fix cold boots and kernel lifecycle](https://github.com/BOPOHA/BPI-M4-fc/blob/ab5375ceb78cdb83a8fc3d87257f866a81a5d35d/docs/hardware/spi-cpu-flash-2026-11-26.md)
- [Server/Media handoff and full-chip readback](https://github.com/BOPOHA/BPI-M4-fc/blob/ab5375ceb78cdb83a8fc3d87257f866a81a5d35d/docs/hardware/spi-modes-flash-2026-11-27.md)
- [Backup and installed-header audit](https://github.com/BOPOHA/BPI-M4-fc/blob/ab5375ceb78cdb83a8fc3d87257f866a81a5d35d/docs/hardware/spi-backup-2026-11-26.md)
- [USB recovery observations](https://github.com/BOPOHA/BPI-M4-fc/blob/ab5375ceb78cdb83a8fc3d87257f866a81a5d35d/docs/hardware/usb-rescue-2026-11-27.md)

Vendor redistribution rights and new hardware qualification remain unresolved.
Build checks do not establish either. No release binary is published by this cleanup.
