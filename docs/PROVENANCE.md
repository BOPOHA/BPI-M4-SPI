# Source provenance and licenses

Extracted from [BPI-M4-fc at ab5375ceb78cdb83a8fc3d87257f866a81a5d35d](https://github.com/BOPOHA/BPI-M4-fc/blob/ab5375ceb78cdb83a8fc3d87257f866a81a5d35d).
Imported copyright and GPL notices remain in source files; see [LICENSE.md](../LICENSE.md).

## U-Boot adaptations

`src/u-boot/sd.c` retains the required SD PLL, clock and pad sequence from
`rtk_sdmmc_init()` in the same GPL BSP's
`u-boot-rtk/drivers/mmc/rtksdmmc_rtd1395.c`.
Reference SHA-256:
`1f2092e562440092e69198c4079b272346fa8b9013002e96cc0a61d683843006`.

`src/u-boot/cpus.c` adapts the secondary-core release sequence from
`arch/arm/cpu/armv8/rtd1395/cpu.c` in that BSP. Copyright Tom Ting (2015) and
GPL-2.0-only remain in the file. Reference SHA-256:
`5ed1fb35e666905518ad2e6dfe00ce540f56a86929f4c2bfa2c68fc9d077de8a`.

`patches/u-boot/0002-use-32-bit-secondary-core-mailbox.patch` adapts the upstream ARMV8 holding pen to the
RTD1395 32-bit mailbox using the BSP's `arch/arm/cpu/armv8/start.S`.
Reference SHA-256:
`7bdb4c53dec4320247854fc28e1c000e965874b0e3ccd6bb7dcb0275c0eb31d3`.

These references document attribution; builds use the prepared upstream sources
and retained files. The standalone SPI build does not need a separate BSP checkout.

`src/spi/sd-init.c` adapts cold SD initialization from the same RTD1395 driver;
`src/spi/lowlevel.S` adapts ACTLR/GIC entry setup from the same Realtek `start.S`.
Their Realtek attribution and GPL-2.0-only notices are retained. The SPI target uses the retained `sd.c`, `cpus.c` and mailbox patch.

`src/spi/spi-tools.c` implements the pinned packaging format using current
OpenSSL APIs. The mode-97 key derivation was reconstructed from the BSP's native
`obfuse_h` reference and verified against it inside Mock. `spi-layout.c` follows
the RTD1395 `bindAllBins.c` layout and is compared with the rebuilt reference.
Vendor Perl RSA formatting and native-host reference generators are confined to
build comparisons, not shipped as runtime helpers. See [SPI-BOOT.md](SPI-BOOT.md)
for pinned identities, retained firmware hashes and format details.

The SPI RPM additionally contains the pinned vendor ARM/MIPS ROM and encrypted
FSBL blobs. The BSP does not establish their separate redistribution terms;
`LicenseRef-Realtek-Firmware` records that unresolved part of this local research
package. Do not infer public firmware distribution rights from a successful build.


Official U-Boot and vendor BSP inputs are pinned in scripts/build-spi.bash.
Archives and private chip dumps are not tracked.
