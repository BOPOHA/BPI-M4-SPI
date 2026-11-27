# SPI loader build and boot contract

Build the modern loader with current Fedora tools as a regular user:

```sh
make checks
make spi SPI_DRAM_GIB=2 JOBS=8
```

Use SPI_DRAM_GIB=1 only for a 1 GiB board. `make srpm-spi` prepares a
self-contained source RPM without compiling it. Compilation happens through
/usr/bin/mock; no build command flashes hardware. No OS/image checkout is needed.
The vendor-only baseline builder has been retired.

Inputs are official U-Boot v2027.01-rc3 commit
`ee1b3477e3af6af1ae9b8f98bc84d2cc6f32a3a6` and Realtek BSP commit
`25f5b88ec4ba34029f964693dc34028b26e6c67c`. Sources are archived from pinned
Git objects, excluding SDK toolchains. This pins source identity without
claiming detached release-signature verification.

| Product | Path |
| --- | --- |
| Source RPM and checksum | out/srpms/spi.src.rpm, spi.src.rpm.sha256 |
| Candidate RPM | out/spi/bpi-m4-spi.rpm |
| Loader, configuration and ELF | out/spi/u-boot.bin, uboot.config, u-boot |
| Raw primary boot region | out/spi/spi-boot-region.bin |
| Integrity and layout | out/spi/SHA256SUMS, spi-layout.json |
| Build audit and tool inventory | out/spi/build-audit.json, installed_pkgs.log |
| Complete build logs | work/rpms/spi/result/ |

```sh
(cd out/srpms && sha256sum --check spi.src.rpm.sha256)
(cd out/spi && sha256sum --check bpi-m4-spi.rpm.sha256 SHA256SUMS)
```

The raw region is not a complete SPI-chip image. Preserve private full-chip
backups outside checkout cleanup paths. Follow [SPI-UPDATE.md](SPI-UPDATE.md)
for the manual update procedure.

The boot ROM and retained DDR/FSBL firmware start one modern SPI U-Boot, which
loads /boot/extlinux/extlinux.conf from an SD ext4 root. Kernel, DTB and generic
initramfs use versioned /boot/bpi-m4/ bundles. No card UUID, root size or kernel
hash is embedded in SPI. The loader validates partition bounds, passes PARTUUID,
SD CID and write bounds, adjusts installed RAM, and provides a CID-derived MAC.
SD writes and persistent U-Boot environment writes remain disabled.

FSBL enters CPU0 at 0x100000 and secondary CPUs at 0x30000. The loader verifies
the 112-byte secondary waiting loop and uses the 32-bit mailbox at 0x98007f30.
The matching kernel entry shim handles EL3-to-EL2 startup. Server requires
inactive ACPU/RPC; Media additionally validates reserved memory and publishes
bananapi,spi-media-handoff = "v1". Linux loads Media firmware from SD.
See [BOOT-CONTRACT.md](BOOT-CONTRACT.md) and the extlinux example in packaging/files/.

Packaging checks cover configuration, entry address, payload limits, retained
DDR/FSBL hashes, secondary-entry disassembly, and byte comparisons with rebuilt
vendor reference generators/packer. Current OpenSSL APIs implement the required
SHA padding, mode-97 obfuscation and RSA formatting; obsolete SDK tools do not
generate shipped artifacts. Required vendor references and encrypted early
firmware are retained. See [PROVENANCE.md](PROVENANCE.md) for attribution.

New candidates require physical qualification. Media output, eMMC boot and USB
SPI recovery are unqualified. U-Boot reset and mode switching require physical
power cycling. Automatic failed-boot rollback and signature-enforced kernel boot
are not implemented. Firmware redistribution rights remain unresolved.
