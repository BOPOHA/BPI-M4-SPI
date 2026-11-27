Independent SPI extraction validation, 2026-11-27

Four retained C fault harness tests passed in the SPI-only Mock checks SRPM.
The complete standalone SPI SRPM/RPM build then passed U-Boot compilation,
reference packer/crypto comparisons, retained vendor DDR/FSBL checks, raw layout,
and the exact 112-byte secondary-entry and relocation/disassembly checks.
See SPI-BUILD-2026-11-27.json for the source and artifact identities.

The build uses no OS sources, Linux patches or image receipts. U-Boot's official
GitHub mirror supplied the pinned commit after the canonical server's lazy fetch
stalled. Git probing disables lazy fetching for existence checks; exact commit
fetches still pin all archived source. Linux/mailbox compatibility is unchanged.

Artifacts under out/spi are raw build candidates, not private full-chip backups.
No device was written and changed artifacts have not booted on hardware. Keep
private backups outside this checkout and cleanup paths. Vendor binary release
rights must be reviewed before publicly uploading firmware artifacts.
