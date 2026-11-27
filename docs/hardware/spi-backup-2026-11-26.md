# BPI-M4 SPI backup and board-specific candidate, 2026-11-26

Two independent reads of the installed 16 MiB SPI matched in RAM. Each was
written to SD, loaded into a separate buffer and compared over all 16,777,216
bytes. Host copies also matched and passed exact-size and SHA256 checks.
During this backup, physical SPI was not erased or programmed. Recovery is not yet validated;
the modern loader's one-CPU RAM result remains unchanged.

## Backup identity and evidence

Board: the user's 2 GiB BPI-M4 / RTD1395, SPI ID `00ef4018`, vendor U-Boot
2015.07 dated `Feb 01 2024 - 02:43:16 +0000`. After a power cycle, the
ACPU-clock word was `40000024` and RPC mailbox was zero. Both reads reported
mapped source `0x88100000`. The installed bootcode begins with AArch64 branch
word `1400000a`, not an empty buffer.

| File | Bytes | SHA256 |
| --- | --- | --- |
| `out/spi/backups/spi-backup-20261126-a.bin` | 16,777,216 | `40585e79626f72b3364658a1ce170101debc2a54e1bab347fdf80eab147fc286` |
| `out/spi/backups/spi-backup-20261126-b.bin` | 16,777,216 | `40585e79626f72b3364658a1ce170101debc2a54e1bab347fdf80eab147fc286` |
| [Normalized UART transcript](spi-backup-2026-11-26.txt) | — | `4f585e1023b2104cc6938741c60f2d80b4a89233b37efb4c814fa1a590686da3` |
| `out/spi/backups/uart-spi-backup-check-2026-11-26.log` | — | `d975e00fe607bae496754dedf77828e58225498bf029a2ea8b3e95be91dd9ea2` |

The original UART log includes an opening stream of empty prompts; one `help`
command restored normal responses. The transcript starts at the actual
`version` command and removes terminal controls. The source of the opening
noise was not established. SPI contents and key/factory data stay in ignored
local artifacts, outside source control.

Verified copies of both chip backups, the full candidate, its manifest and
the UART log also reside outside the checkout at
`/home/eastern/.local/share/bpi-m4/spi-backups/2026-11-26/`, with a checked
`SHA256SUMS`. This matters because `make clean-hard` uses `git clean -dfX`
and removes ignored `out/` and `work/` artifacts, including their backups.
The external directory is retained across that cleanup.

## Installed layout versus the build

The first 196,608 bytes, including both packer ROM slots, are entirely `ff`.
The following 2,048-byte parameter area is populated, with SHA256
`7e35987f8f92da668d357fce577b474affce65966d6b63fc452573ee405f5ec3`.
The build's populated BSP ROM slots and zero parameter slot therefore must
not replace this board's installed prefix.

The 96-byte header at `0x30800` matches the UART dump. Native OpenSSL AES-128-ECB
decoding used the mode-97 derivation in `src/spi/spi-tools.c` and the pinned
vendor module/SCK/vendor inputs. The decoded header's last 32 bytes match
SHA256 of its first 64 bytes. This verifies the header decoding, not the
board's complete OTP/security state or a future SPI boot.

Its size fields are DDR **2,432**, U-Boot **527,256**, FSBL **30,720**; the other
fields are zero. The cold-boot UART reports U-Boot load size `0x80da0`, or
**527,776**: the header's U-Boot size **plus 520**. The FSBL extracted using
that convention matches the retained blob exactly. This supports retaining
the vendor `boot_enc` subtraction of 520, even for plaintext U-Boot.

| Component | Installed byte offset | Installed bytes | Comparison |
| --- | --- | --- | --- |
| Encrypted header | 198,656 | 96 | SHA256 `85b07caa019c00955c5a3c78d2b49d451ec3d9ef949d582955282f87c4951755` |
| DDR settings + record | 198,752 | 2,952 | Exact candidate match: `3318d415ec07d54c7e27a9f2d578ac83b2d5492e5c5705b8df42508a5b11a8fd` |
| Vendor U-Boot component | 201,704 | 527,776 | Replaced by the smaller modern component |
| FSBL | 729,480 (`0xb2188`) | 31,240 | Exact candidate match: `cd201712e767b8de74fe421b9fedde1fe3f31255115a174012fc7e3050716228` |

The new header also passes its own SHA256 check. Its U-Boot field is 303,960;
adding 520 gives the complete 304,480-byte component, containing the
304,392-byte v2027.01-rc3 loader, padding and digest. Its unchanged FSBL moves
to offset 506,184. The [subsequent physical SPI cold boot](spi-flash-2026-11-26.md)
confirmed acceptance of this layout on the tested board and loaded the full
304,480-byte modern U-Boot component.

## Prepared full-chip candidate

`out/spi/spi-chip-candidate-20261126.bin` is a local 16,777,216-byte candidate,
SHA256 `bf2d0bbb83c827e4cbd0e35ccaf310f9e3003ad9ab739383d42a3f6a02663fef`.
It starts from backup A and overlays only bytes **198,656–537,423** from the
537,424-byte `spi-boot-region.bin` (SHA256
`9a4ce12a88799002ad4d2965321f633b7af3233ea1a9dd95aeed1c3f8d97259f`).
The replacement length is 338,768 bytes (`0x52b50`); its exclusive end is
`0x83350`. Native `cmp` verifies all three ranges:

- Bytes 0–198,655: identical to the installed backup, including ROM slots and parameters.
- Bytes 198,656–537,423: identical to the built header/DDR/U-Boot/FSBL span.
- Bytes 537,424–16,777,215: identical to the backup, including factory at `0x200000`.

Old trailing firmware bytes are preserved; the new header selects the new
component positions. These logical replacement bounds are not an approved
erase/program range. Erase-sector alignment and restoration of surrounding
bytes must be included in a later reviewed physical-write procedure.

The accompanying `spi-chip-candidate-20261126.json` records inputs, ranges and
unqualified recovery/SPI-boot status. The following native commands reproduce
the overlay into a **regular host file** after verifying those two inputs.
Use a new output filename if this one already exists.

```sh
set -euo pipefail
backup=out/spi/backups/spi-backup-20261126-a.bin
region=out/spi/spi-boot-region.bin
candidate=out/spi/spi-chip-candidate-20261126.bin
test "$(sha256sum "$backup" | cut -d ' ' -f1)" = 40585e79626f72b3364658a1ce170101debc2a54e1bab347fdf80eab147fc286
test "$(sha256sum "$region" | cut -d ' ' -f1)" = 9a4ce12a88799002ad4d2965321f633b7af3233ea1a9dd95aeed1c3f8d97259f
test ! -e "$candidate"
cp "$backup" "$candidate"
dd if="$region" of="$candidate" iflag=skip_bytes oflag=seek_bytes \
   skip=198656 seek=198656 bs=1M conv=notrunc status=none
test "$(stat -c %s "$candidate")" = 16777216
cmp -n 198656 "$backup" "$candidate"
cmp -n 338768 -i 198656:198656 "$region" "$candidate"
cmp -i 537424:537424 "$backup" "$candidate"
sha256sum "$candidate"
```

## FAT error and completed repair

The initial `fatwrite` filename began with `/`. Vendor U-Boot stored that slash
in both the long filename and short alias `/SPI-B~1.BIN`; Linux directory
listing returned an I/O error. Both corrected bare names `SPIA1126.BIN` and
`SPIB1126.BIN` were nevertheless readable directly, and their host copies were
secured before any repair.

The user exported the unmounted boot partition into
`work/spi-backup-audit/sd-boot-original.img`, 268,435,456 bytes, SHA256
`c40ee4a4eecc93a7896019ef7328c8da210353628f2b7829502436797eaf4c2b`.
Native `fsck.fat -n -v` found the invalid alias and a stale free-cluster count
(515,591 instead of 417,287). Repair was first tested on a regular-file copy:
`fsck.fat -a -v` renamed the entry to `FSCK0000.000` and corrected the counter;
the following read-only check was clean. Native mtools extraction and `diff`
verified every existing valid boot file and both backups remained unchanged.
The recovered third backup also matched A byte for byte.

The user then ran the same repair on unmounted `/dev/sdb1`, followed by
`fsck.fat -n`; the reported changes matched the offline trial and the final
check was clean. An agent read-only remount confirmed directory listing works,
all saved boot files still match, and all three SPI files have the same backup
SHA256. The partition was unmounted again. No SD image was reflashed.

## Recovery decision

The user supplied an SD/UART `spiloader-*.bin` procedure. The cited
[BPI-M4 forum post](https://forum.banana-pi.org/t/spi-rom-update-issue/12264/7)
confirms an update through an already running vendor SPI U-Boot. It does not
demonstrate recovery if the SPI bootloader itself fails.

The [official getting-started page](https://docs.banana-pi.org/en/BPI-M4/GettingStarted_BPI-M4)
sets SW2=0 for Linux and SW2=1 for a Windows USB tool that flashes Android/eMMC.
Earlier local board observations in `../BPI-M4-PoC/docs/HARDWARE.md` reached
SPI U-Boot at SW2=0 without SD; SW2=1 failed at early hardware settings.
Neither establishes an independent, working SPI restore path. USB/ROM recovery
can be investigated; otherwise a correctly configured external SPI programmer
must be verified against the actual chip/package before programming.

After this backup was committed, the user explicitly approved proceeding with
the SPI write and accepted the unverified recovery risk. That authorization
supersedes the earlier recovery prerequisite for this attempt. The reviewed
physical span is `[0x30000, 0x84000)`, restoring all surrounding parameter/tail
bytes from the verified backup. No vendor updater was executed during backup;
programming and first cold-boot results are in the
[subsequent physical SPI record](spi-flash-2026-11-26.md).
