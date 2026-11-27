# SPI backup and UART update

The 2 GiB board's verified full-chip image is saved outside the checkout:

```text
~/.local/share/bpi-m4/spi-backups/2026-11-27/modes-update/
```

`spi-after.bin` is the new 16,777,216-byte image. Physical SPI readback matched
every byte and returned CRC32 `845cc5f7`. Its SHA256 is
`f7ae1a4130351fa77f7828e65b549370ef221135a59a8153ef130429da8ba651`.
`spi-before.bin` preserves the working four-core loader from before this update,
CRC32 `7bb5492e`, SHA256
`64ffc1fdd40b6b98bdd743f0427bd450da481c836c819ec2c6e54f60de48e571`.
The directory also contains the vendor RAM writer, handoff, CRC helper, hashes
and build audit. `out/spi/update-20261127/` is a disposable checkout copy.
The durable directory survives `make clean-hard`.

These full-chip images include this board's parameters and factory data.
They are backups for this tested 2 GiB board, not universal installation images.
The RPM's `spi-boot-region.bin` is a build product with a different ROM prefix;
do not write it directly over a chip. New releases must be combined with a
verified backup of the actual board, preserving its prefix and factory tail.

## RPM installation versus flashing

`make spi SPI_DRAM_GIB=2` builds `out/spi/bpi-m4-spi.rpm` in Mock. Installing
that RPM only places firmware under `/usr/lib/bpi-m4/spi-2GB/`; it does not
program flash. The modern loader has no SPI write command. The current Linux
kernel has no usable SPI-NOR/MTD flash interface, and an OS updater is not yet
implemented. UART is the tested update path. A future OS updater needs a
qualified flash driver, board/layout checks, backup, bounded erase/write and
full readback verification; root privileges will be required.

## Prepare the files on the host

This procedure uses the tested release's write span `[0x30000, 0x84000)`;
it is not a recipe for arbitrary firmware sizes or other board variants.
Choose `SPI_BASELINE` as the verified image currently installed and `SPI_IMAGE`
as the image to install. After the recorded update, both examples below name
the current saved image. To restore the previous loader, select `spi-before.bin`
as `SPI_IMAGE` while keeping `spi-after.bin` as `SPI_BASELINE`.

```sh
SPI_BACKUPS="$HOME/.local/share/bpi-m4/spi-backups/2026-11-27/modes-update"
(cd "$SPI_BACKUPS" && sha256sum --check --strict SHA256SUMS)
SPI_BASELINE="$SPI_BACKUPS/spi-after.bin"
SPI_IMAGE="$SPI_BACKUPS/spi-after.bin"
SPI_STAGE="$PWD/work/spi-uart"
mkdir -p "$SPI_STAGE"
cp "$SPI_BASELINE" "$SPI_STAGE/spi-before.bin"
cp "$SPI_IMAGE" "$SPI_STAGE/spi-after.bin"
cp "$SPI_BACKUPS"/{vendor-uboot.bin,vendor-handoff.bin,crc32.bin} "$SPI_STAGE/"
```

Verify sizes and preservation before staging. Run in Bash with `set -e` so a
failed comparison stops the sequence. The two `cmp` commands cover everything
outside the programming span, including parameters and factory data.

```sh
set -e
test "$(stat -c %s "$SPI_STAGE/spi-before.bin")" -eq 16777216
test "$(stat -c %s "$SPI_STAGE/spi-after.bin")" -eq 16777216
cmp -n $((0x30800)) "$SPI_STAGE/spi-before.bin" "$SPI_STAGE/spi-after.bin"
cmp -i $((0x84000)) "$SPI_STAGE/spi-before.bin" "$SPI_STAGE/spi-after.bin"
(cd "$SPI_STAGE" && sha256sum *.bin > SHA256SUMS)
# IEEE CRC32 from gzip's trailer on the Fedora x86_64 build host:
spi_crc32() { gzip -n -c "$1" | tail -c 8 | od -An -tx4 -N4 | tr -d ' \n'; }
spi_crc32 "$SPI_STAGE/spi-before.bin"; printf '\n'
spi_crc32 "$SPI_STAGE/spi-after.bin"; printf '\n'
```

Keep those CRC values for the UART checks. Check the board's root free space
with `ssh fedora@192.168.1.241 'df -h /; lsblk'` before copying about 33 MiB.
Staging needs ordinary SSH access, not root:

```sh
ssh fedora@192.168.1.241 'mkdir -p spi-uart'
scp "$SPI_STAGE"/*.bin "$SPI_STAGE/SHA256SUMS" fedora@192.168.1.241:spi-uart/
ssh fedora@192.168.1.241 'cd spi-uart && sha256sum --check --strict SHA256SUMS && sync -f .'
```

Shut down Linux cleanly through the root UART shell. Keep SW2=0, SD inserted
and power stable. Start the terminal before power-on:

```sh
picocom -b 115200 -T 5000000 /dev/ttyUSB0
```

At the modern loader's one-second countdown, press a key to stop autoboot.
Run each UART command separately and wait for its prompt; do not paste a block.
The 5 ms character delay prevents dropped characters seen on this console.

## Enter the RAM writer from modern U-Boot

Load the writer and helpers from ext4 root partition 2. After each load check
the returned size and CRC. These addresses and helper sizes are specific to
the saved bundle.

```text
ext4load mmc 0:2 0x20000000 /home/fedora/spi-uart/vendor-uboot.bin
crc32 0x20000000 0x80da0
ext4load mmc 0:2 0x21000000 /home/fedora/spi-uart/vendor-handoff.bin
crc32 0x21000000 0x4c
ext4load mmc 0:2 0x22000000 /home/fedora/spi-uart/crc32.bin
crc32 0x22000000 0x58
ext4load mmc 0:2 0x24000000 /home/fedora/spi-uart/spi-before.bin
crc32 0x24000000 0x1000000
ext4load mmc 0:2 0x26000000 /home/fedora/spi-uart/spi-after.bin
crc32 0x26000000 0x1000000
```

Writer CRC is `4a1fa242`, handoff `53b3d035`, helper `603b2e1b`; compare both
full-chip CRCs with the host values. Then run `go 0x21000000`. The handoff copies
exactly 527,776 writer bytes to `0x100000` and enters the vendor console in RAM.
Wait for `BPI-M4(SPI)>`. The temporary 2015 banner is the programming tool;
it does not add a persistent boot stage. This path does not run an automatic
vendor `spiloader` updater or require an obsolete toolchain.

If the board already runs original vendor U-Boot 2015, skip the RAM writer
handoff. Copy the selected baseline, target and helper to the first FAT
partition as `SPIOLD.BIN`, `SPINEW.BIN`, `SPICRC.BIN`. At its console, disable
caches as below, then load each file and check its size:

```text
base 0
icache off
dcache off
fatload sd 0:1 0x24000000 SPIOLD.BIN
printenv filesize
fatload sd 0:1 0x26000000 SPINEW.BIN
printenv filesize
fatload sd 0:1 0x22000000 SPICRC.BIN
printenv filesize
```

Expected sizes are `1000000`, `1000000`, `58`. Original SPI must have its own
[verified full-chip baseline](hardware/spi-backup.md); the saved CPU-fix
baseline will not match an original 2015 chip.
Use a compatible legacy SD boot image for later Linux boot, or prepare the
common image before switching to the modern loader. A common image alone
does not satisfy the original loader's SD second-stage boot contract.

## Preflight in the vendor console

```text
base 0
icache off
dcache off
rtkspi init
md.l 0x98000010 1
md.l 0x9801a108 1
md.l 0x22000000 0x16
```

Require chip ID `00ef4018`, capacity `01000000`, 4 KiB erase support, ACPU
register `40000024` and RPC `00000000`. Check the CRC helper's complete words:

```text
22000000: d2a44027 b94004e0 b94008e1 12800002
22000010: 52906403 72bdb703 34000181 38401404
22000020: 4a040042 52800105 12000046 53017c42
22000030: 34000046 4a030042 710004a5 54ffff61
22000040: 71000421 54fffec1 2a2203e2 b90000e2
22000050: d2800000 d65f03c0
```

Read the actual chip and compare it with the selected baseline before erasing:

```text
rtkspi read 0x0 0x28000000 0x1000000
cmp.b 0x24000000 0x28000000 0x1000000
mw.l 0x22010000 0
mw.l 0x22010004 0x26000000
mw.l 0x22010008 0x1000000
go 0x22000000
md.l 0x22010000 3
```

Require `Total of 16777216 byte(s) were the same` and the target's host CRC
followed by `26000000 01000000`. For the saved new image that CRC is `845cc5f7`.
On mismatch, stop and obtain a verified current backup; do not bypass this gate.
If `cmp.b 0x26000000 0x28000000 0x1000000` also matches all bytes, the selected
image is already installed and no erase/write is needed.

## Erase, flash, then verify

All `rtkspi` arguments are bytes. Keep power unchanged throughout. Run one
command at a time and wait for `BPI-M4(SPI)>`.

```text
mw.l 0x2b000000 0xffffffff 0x15000
rtkspi erase 0x30000 0x54000
rtkspi read 0x30000 0x2a000000 0x54000
cmp.b 0x2a000000 0x2b000000 0x54000
```

Require adjusted erase boundaries `88130000` through `88184000` and all
344,064 bytes equal to the `ffffffff` buffer. Only then program:

```text
rtkspi write 0x30000 0x26030000 0x54000
rtkspi read 0x0 0x2a000000 0x1000000
cmp.b 0x26000000 0x2a000000 0x1000000
mw.l 0x22010000 0
mw.l 0x22010004 0x2a000000
mw.l 0x22010008 0x1000000
go 0x22000000
md.l 0x22010000 3
```

Require all 16,777,216 bytes equal and the target CRC followed by
`2a000000 01000000`. This checks the boot payload and all preserved chip data.
If either check fails, keep power on and retain both old-image RAM buffers.
Restoration uses the same checked erase sequence, then writes the old span from
`0x24030000`; full readback must match `0x24000000` and its recorded CRC before
power-off. If flash writes remain unreliable, leave the board powered and use
an independent recovery method. UART cannot recover a chip that no longer
reaches a working console.

SW2=1 also exposes an early Realtek USB interface (`0bda:1195`) on this board.
The official Windows tool documents Android/eMMC flashing; restoration of SPI
through that interface has not been verified. See the
[USB rescue observation](hardware/usb-rescue-2026-11-27.md) for the evidence
and the operator's proposed Windows recovery method. Use SW2=0 for normal boot
and this UART update procedure.

After successful verification, disconnect power for at least five seconds and
reconnect. Confirm one modern U-Boot, Linux CPUs `0-3`, about 1.9 GiB RAM in
Server mode, matching boot bundles and working networking. The new loader
publishes `/proc/device-tree/chosen/bananapi,spi-media-handoff` as `v1`.
Media firmware/output qualification is separate from a successful Server boot.
