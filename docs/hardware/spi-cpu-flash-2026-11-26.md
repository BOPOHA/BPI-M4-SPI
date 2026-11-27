# Persistent SPI CPU fix, 2026-11-26

The user approved the programming-path validation, CPU-fix SPI update, repeated
boots and kernel update/fallback testing. The CPU-fix candidate was programmed;
erase verification, a full 16 MiB readback comparison and CRC32 passed.
Three physical cold boots and three Linux reboots passed with all four CPUs
online. Kernel installation, fallback, unused-version removal and reinstallation
also passed. The board is running release 2 with release 1 retained as fallback.

## Verified RAM programming path

The installed modern U-Boot has no SPI write command. The original vendor
U-Boot component was extracted from the previously verified chip backup and
loaded into RAM only. Its 527,776 bytes match the backup component exactly,
with SHA256 `5be0f6cc5699b70d9354a80ac076e61c883d6631bbf684b060c17bd37b883793`
and CRC32 `4a1fa242`. No legacy compiler or vendor SDK was installed.

The existing native RAM handoff was assembled for that exact component size.
It is 76 bytes, SHA256
`a661611e5464ac5c1a5ace513389d3fb54f87131f24c3da63355722907527968`,
CRC32 `53b3d035`. The 88-byte returning CRC helper was unchanged, CRC32
`603b2e1b`; all 22 words matched after the vendor handoff.

Before copying files, the running board had 328 MiB free on its 1 GiB root
partition. Staging the vendor component, helpers and two full chip images left
295 MiB free. Normal SSH file copies into ext4 p2 passed SHA256 and filesystem
flush checks; no SD raw flashing or partition changes were needed.

The installed modern loader retrieved the files from p2 and verified their CRCs
before entering the vendor loader. The vendor banner is temporary programming
execution in RAM; it is not a second persistent stage in the SPI boot chain.
The console identified the expected chip ID `00ef4018`, 16 MiB capacity and
4 KiB erase support. Caches were disabled, ACPU clock bit remained clear
(`40000024`) and RPC remained zero.

## Exact programmed image and checks

The current installed chip was read into `0x28000000` and compared with the
saved working modern image at `0x24000000`: all 16,777,216 bytes matched.
The proposed full image at `0x26000000` passed the returning CRC helper after
the vendor initialization, with expected CRC32 `7bb5492e`.

| Artifact | Identity |
| --- | --- |
| Previous working chip | SHA256 `bf2d0bbb83c827e4cbd0e35ccaf310f9e3003ad9ab739383d42a3f6a02663fef` |
| CPU-fix full chip candidate | SHA256 `64ffc1fdd40b6b98bdd743f0427bd450da481c836c819ec2c6e54f60de48e571`, CRC32 `7bb5492e` |
| Aligned programming span | 344,064 bytes; SHA256 `ea4181691e48dfe878351ded24414c821c1b480df906d7bdecce7b50e547190b`, CRC32 `5d3e98ed` |

The physical span is `[0x30000, 0x84000)`, as in the first SPI update. The
logical replacement ends at `0x834d0`; surrounding parameter and tail bytes are
restored from the working chip image. The vendor DDR/FSBL inputs are unchanged.
Factory data at `0x200000` is outside the write span. Both working-image RAM
buffers remained available for restoration during programming.

Erase began at **21:13:57 UTC**. A read of the erased span into `0x2a000000`
matched an `ffffffff` buffer over all 344,064 bytes. Programming began at
**21:14:53 UTC** and its completed prompt was observed at **21:15:10 UTC**:

```text
rtkspi erase 0x30000 0x54000
rtkspi write 0x30000 0x26030000 0x54000
```

Automatic approval review failed before the subsequent agent readback command
was sent; its retry was rejected and required manual verification. The agent
released UART without resetting the board. The operator then performed the
[full readback and CRC checks](spi-cpu-flash-readback-2026-11-26.txt), confirming
all 16,777,216 bytes matched the candidate and:

```text
22010000: 7bb5492e 2a000000 01000000
```

The result was confirmed at **21:20:44 UTC**. This verifies the programmed
payload and all preserved prefix/tail/factory bytes. The readback was performed
by the operator, separately from the agent's earlier read-only validation.

The candidate JSON under `out/spi/` records programming and verification.
The existing RAM-test snapshot remains archived separately; the programming
record and evidence are retained under the private durable directory
`~/.local/share/bpi-m4/spi-backups/2026-11-26/cpu-fix/programming/`.

## Subsequent validation

The operator performed three physical power cycles with the SD card inserted
and charger connections unchanged. Each boot used one modern SPI U-Boot,
installed the 112-byte secondary waiting loop automatically, and reached Fedora
with CPUs `0-3`, 1.9 GiB RAM, zero failed systemd units and three successful
public-IP pings. Each also passed a 64 MiB-per-core two-pattern test and 4,096
shared atomic handoffs. This checks 256 MiB and coherency, not all installed RAM.

| Cold boot | Boot ID | Kernel startup | Userspace startup |
| --- | --- | --- | --- |
| 1 | `94fe25cb-7223-495d-97d1-cb1bb4538b64` | 1.663 s | 13.097 s |
| 2 | `9e5d9917-9723-41cb-8073-3222dab2ad1e` | 1.651 s | 13.229 s |
| 3 | `a04243e3-8699-4d4f-8700-ee9fd3bf04fb` | 1.646 s | 16.469 s |

All three ran `7.3.5-bpi_m4_fc_p10.r1.fdc85ca9b5a0`. These
`systemd-analyze` measurements exclude ROM, FSBL and U-Boot time.

The release-2 kernel RPM built in host Mock and was installed through DNF in
the root UART shell with repositories disabled. Its SHA256 was verified before
installation; no repository metadata or package downloads were needed. DNF
co-installed both kernel versions, and the successful post-transaction hook
created a complete release-2 boot bundle and selected it in extlinux. The 1 GiB
root had 286 MiB free before installation and 256 MiB afterward.

The first Linux reboot ran release 2; the second ran release 1 after explicit
fallback selection with `bpi-m4-bootctl set-default`. Both passed the same
four-core, memory/coherency, boot-bundle and networking checks. Source hashes
also matched the packaged kernel artifacts, and the running ABI had a matching
module directory.

| Linux reboot | Selected release | Boot ID | Kernel / userspace startup |
| --- | --- | --- | --- |
| 1 | release 2 | `14ea345d-df73-44e1-b2bc-f0dbb7d9c8bb` | 1.646 s / 13.198 s |
| 2 | release 1 fallback | `5490c66b-5bca-4826-9f60-a9a936fb7ccf` | 1.652 s / 13.153 s |
| 3 | release 2 after reinstallation | `5894fd9c-c224-4353-aa32-ce9eaef09ccf` | 1.650 s / 13.134 s |

While release 1 was running, DNF removed only the unused release-2 package. Its
boot bundle, immutable sources and module directory disappeared; release 1
remained selected and passed both bundle checks and `rpm -V`. Root free space
returned to 283 MiB. Reinstallation of the same verified release-2 RPM restored
both entries, selected release 2 and passed `rpm -V` for both installed kernels.
The third Linux reboot then ran release 2, passed all checks again and left
252 MiB free on root. No partition resizing, SD raw flashing or on-board kernel
compilation was performed.

The combined UART recorder contains exactly six modern U-Boot banners, six
automatic secondary-loop installations, eighteen secondary-CPU starts, six
Linux login banners and three Linux watchdog restarts. There were no additional
uncommanded resets during these tests. The
[selected UART and Linux evidence](spi-cpu-boots-2026-11-26.txt) records all six
cycles and the update/removal checks. Full logs and candidate metadata remain
in the durable private directory above.

Release identities:

| Version | RPM | Kernel ABI |
| --- | --- | --- |
| Baseline | `bpi-m4-kernel-7.3.5-1.fdc85ca9b5a0.bpi_m4.fc45.aarch64` | `7.3.5-bpi_m4_fc_p10.r1.fdc85ca9b5a0` |
| Tested update | `bpi-m4-kernel-7.3.5-2.2d4edadf8d89.bpi_m4.fc45.aarch64` | `7.3.5-bpi_m4_fc_p10.r2.2d4edadf8d89` |

The update uses the same upstream kernel and board code, with a new numeric RPM
release and distinct ABI to exercise the complete update path. RPM SHA256 is
`25d70f5f6ba0eb9975d5857811a1ed59f2565e270bc6687a9785833af653a0e6`;
SRPM SHA256 is
`ec1ec6a1eb75b2d4d7c9e48e3d47b8d3b36018c5ee16af3a47d665096b382c79`.
The local RPM was unsigned; its independently checked SHA256 identified the
test artifact. This was a local DNF lifecycle test, not a public repository
update. [COPR preparation](../COPR.md) describes the native kernel build path;
no public project or build has been submitted.

The release-2 Mock build and its package checks passed. The earlier CPU-fix
development regression run passed 50 of 51 test methods; one flash-default
method failed three subcases because the existing local Makefile selects SPI
image names while the tests expect legacy names. That pre-existing Makefile
edit was retained separately and was not included in this qualification commit.
The regression log is `work/rpms/checks/result/build.log`, also archived with
the programming evidence. This does not change the six successful hardware
boot tests or the verified kernel transaction results.

## Remaining limits

These results qualify server SMP startup and Linux watchdog reboots on this
2 GiB board with inactive ACPU/RPC. Media startup, eMMC clock initialization,
U-Boot's own reset command, independent failed-SPI recovery and sustained
storage/memory/network load remain unqualified. eMMC still reports probe error
`-22`. Complete firmware boot timing is unmeasured. Fallback is selected
manually; automatic failed-boot rollback is not implemented. Native aarch64
COPR compilation and repository updates still require separate validation.
