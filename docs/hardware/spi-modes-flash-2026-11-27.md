# Common-mode SPI update, 2026-11-27

The user authorized a physical UART update and requested a saved SPI image
and flashing instructions. The profile-aware v2027.01-rc3 loader was programmed
on the same 2 GiB RTD1395 board. The erased span matched all `ffffffff` bytes;
full-chip readback matched the candidate over all 16,777,216 bytes and returned
CRC32 `845cc5f7`. A subsequent physical cold boot reached Server Linux with
all four CPUs and 1.9 GiB RAM. The new `v1` handoff capability was confirmed.

## Identities and preserved data

The source implementation is commit `40f4097`, SPI recipe `20c6108143a9`.
Official U-Boot commit is `ee1b3477e3af6af1ae9b8f98bc84d2cc6f32a3a6`;
vendor BSP commit is `25f5b88ec4ba34029f964693dc34028b26e6c67c`.
The native host build uses current Fedora tools; no vendor SDK or old compiler
was installed. See the [common-mode build record](../common-modes-build-2026-11-27.md).

| Artifact | SHA256 / CRC32 |
| --- | --- |
| Previous full chip | `64ffc1fdd40b6b98bdd743f0427bd450da481c836c819ec2c6e54f60de48e571` / `7bb5492e` |
| New full chip | `f7ae1a4130351fa77f7828e65b549370ef221135a59a8153ef130429da8ba651` / `845cc5f7` |
| Programming span, 344,064 bytes | `ea157d1bfe31e01945958837cda3f14032b3b447e0e1109b42e9564645974a79` / `a46e4dcc` |
| Modern loader, 306,128 bytes | `b976a36a17604e1595cbfafca052f4b06c5df735bfb486d7e6811d358310d91d` |
| Raw boot region, 539,152 bytes | `dc2859c97c2dc44697f3cebe93ba86f5f1c2e668a8260e2115d5ef0fef360d0e` |
| Local SPI RPM | `a078ce3949c3746c9ad5e9aef88d4b3248452befe5d397e7f09f37a89cf4884d` |

The logical replacement is `[0x30800, 0x83a10)` in a full-chip copy of the
verified working baseline. Its physical erase/write span is
`[0x30000, 0x84000)`. Parameter bytes preceding the header and padding after
the payload were retained from the baseline. Native `cmp` checks verified
both outer ranges, DDR settings, retained FSBL and factory data unchanged.
The build's raw ROM prefix was not written over the board's actual prefix.

## Programming and verification

Before staging, the existing 1 GiB root had 251 MiB available; copying about
33 MiB left 218 MiB. Files were copied through ordinary SSH into
`/home/fedora/spi-update-20261127/`, then checked with `sha256sum` and flushed
with `sync -f .`. No SD raw flashing, partition resizing or kernel installation
was performed. The existing release-2 kernel and release-1 fallback remain.

The old vendor writer, 527,776 bytes, and the native 76-byte handoff and
88-byte CRC helper were loaded from ext4 p2 and CRC-checked in modern U-Boot.
The writer ran only in RAM. Its SPI ID was `00ef4018`, capacity 16 MiB and
minimum erase size 4 KiB. Vendor caches were disabled; ACPU register was
`40000024`, RPC `00000000`. All 22 CRC-helper words matched after handoff.
The complete current chip matched the saved baseline at `0x24000000`;
the proposed chip at `0x26000000` returned CRC `845cc5f7` after vendor startup.

The erase/write phase began at **00:05:51 UTC**. The controller reported exact adjusted
boundaries `88130000` and `88184000`. Readback of the erased span matched
344,064 bytes of `ffffffff` before the write:

```text
rtkspi erase 0x30000 0x54000
rtkspi read 0x30000 0x2a000000 0x54000
cmp.b 0x2a000000 0x2b000000 0x54000
rtkspi write 0x30000 0x26030000 0x54000
```

The completed write prompt was observed by **00:06:38 UTC**. The agent then
read and verified the entire chip without resetting the board:

```text
rtkspi read 0x0 0x2a000000 0x1000000
cmp.b 0x26000000 0x2a000000 0x1000000
Total of 16777216 byte(s) were the same
```

The returning helper's final result was:

```text
22010000: 845cc5f7 2a000000 01000000
```

Full readback and CRC were confirmed at **00:08:46 UTC**. Both old-image RAM
buffers were preserved throughout. No automatic approval rejection occurred
during this update.

## Durable backup and operator guide

The exact new full-chip image, previous full chip, programming span, RAM
writer/helpers, SPI RPM, build audit and update record are saved under:

```text
~/.local/share/bpi-m4/spi-backups/2026-11-27/modes-update/
```

`SHA256SUMS` verifies the immutable binary bundle; `UPDATE-SHA256SUMS` covers
the update record, copied guide and UART log. These files are outside checkout
cleanup. The saved new image equals the physical chip by full readback; it is
specific to this board. The [UART guide](../SPI-UPDATE.md) documents staging,
the modern-to-vendor RAM handoff, original vendor-console loading, checked
erase/write/readback and restoration.

The local `bpi-m4-spi` RPM installs firmware files; it does not flash SPI.
Linux SPI-NOR/MTD access and a verified OS updater remain to be developed.
Media startup and output remain separate hardware qualification tasks.

## Physical cold-boot validation

The operator disconnected power for at least five seconds, then reconnected
with SD inserted and SW2=0. UART captured the retained FSBL loading `0x4ac20`
bytes at `0x100000`, one U-Boot `2027.01-rc3` banner dated
`Nov 26 2026 - 23:57:56 +0100`, and direct extlinux boot of the existing
release-2 kernel. There was no SD U-Boot stage. The loader reported
`BPI_M4_SPI_PROFILE: server; ACPU firmware deferred to Linux`, installed the
112-byte secondary loop and released CPUs 1–3. Linux started all four at EL2.

Root UART checks passed:

| Check | Result |
| --- | --- |
| Boot ID | `abc591c5-228b-4193-956a-17a5df7972a7` |
| Kernel | `7.3.5-bpi_m4_fc_p10.r2.2d4edadf8d89` |
| Online CPUs | `0-3` |
| RAM | 1.9 GiB total, 1.8 GiB available |
| Running SPI profile | `server` |
| SPI Media handoff marker | `v1` |
| Boot bundles | Release 1 and release 2 verified by `bpi-m4-bootctl verify` |
| Networking | Three public-IP pings received; 0% loss |
| Failed systemd units | None |
| Root free space | 214 MiB on the existing 1 GiB partition |
| Kernel / userspace startup | 1.677 s / 13.243 s |

Startup timings exclude ROM, FSBL and U-Boot. One physical cold boot was
checked for this update; the preceding CPU-fix loader's repeated boot and
memory/coherency results remain in its separate record. eMMC still reports
the previously known unverified-clock error `-22`. Media startup/output and
the newly assembled common SD image were not tested during this SPI-only
update. The existing SD/kernel was retained. UART was released with Linux
running after successful checks.
