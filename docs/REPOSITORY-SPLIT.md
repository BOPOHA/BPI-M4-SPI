# Approved three-repository split and implementation handoff

The user approved the architecture below on 2026-11-27 and requested this
committed handoff for implementation in a fresh session. Review baseline:
commit `77e8ae3`, with the operator-confirmed common Server image and verified
modern SPI loader. This commit records the plan; extraction, remote repository
creation and COPR submissions have not started.

Keep `BPI-M4-fc` as the historical working project. Create three independent
repositories: `BPI-M4-COPR` for OS RPM sources and COPR builds,
`BPI-M4-IMAGE` for SD-image assembly from Fedora/COPR repositories, and
`BPI-M4-SPI` for standalone bootloader development. `BPI-M4-IMAGE` describes
the repository's specific job more clearly than `BPI-M4-OS`.
SPI's own build-only RPM belongs with its source in the SPI project.
Each repository owns its build, tests, source pins, provenance and documentation.
Their build interfaces are published packages and documented boot contracts;
none requires another source checkout or a shared tools repository.

## Start here in a fresh session

Read this document, `AGENTS.md`, `docs/STATUS.md`, `docs/COPR.md`,
`docs/MODES.md` and the hardware records linked below. Implement the migration
in the sequence described here, keeping all three projects independently
buildable. Use sibling directories under `/home/eastern/code/BOPOHA/`:

```text
BPI-M4-fc/       historical source and working baseline
BPI-M4-COPR/     OS package sources and COPR build configuration
BPI-M4-IMAGE/    Fedora/COPR RPM consumption and image assembly
BPI-M4-SPI/      standalone SPI sources, build and update tools
```

Each new repository needs its own README, dependency instructions, Makefile,
tests, licences/provenance and cleanup rules. Keep the historical repository
intact during extraction. OS/kernel compilation belongs to the RPM project;
the image project builds only storage images. SPI builds never invoke an OS
kernel or image build. Git submodules and sibling checkout paths are not
required dependencies.

Follow `AGENTS.md`: never run agent-side `sudo` or trigger authentication.
Run `/usr/bin/mock` as the regular user. The operator runs privileged host
commands and physical SD flashing manually. Hardware flashing is separate
from this source migration; preserve the established hardware baseline.

COPR account/project and remote repository destinations still need to be
resolved before publication. The approved architecture is not evidence that
an authenticated COPR project already exists. See the publication status and
native-build limitations in `docs/COPR.md`.

## Established baseline: preserve, do not rediscover

| Item | Recorded state |
| --- | --- |
| Board | Original Banana Pi M4, Realtek RTD1395, 2 GiB RAM |
| Userspace / package target | Fedora 45 / aarch64 |
| Local build host | Fedora 45 x86_64, regular-user Mock with current Fedora cross tools |
| Kernel source | Signature-verified kernel.org Linux 7.3.5 plus board patches; pins in `inputs.server.lock.json` |
| Working Server kernel | `7.3.5-bpi_m4_fc_p10.r4.6dd0c134e3bb`; numeric package release currently `4`; release 3 retained as fallback |
| Saved common image | `out/fedora-bpi-m4.raw`, 2,416,967,680 bytes; 256 MiB FAT and 2 GiB ext4 root; contains release 3 |
| Image SHA256 | `9bc296890d8eb805fd14f290e2e93c854cf40e89e7cb1edfa6a3d12421ce83ec` |
| SPI U-Boot | v2027.01-rc3; upstream commit `ee1b3477e3af6af1ae9b8f98bc84d2cc6f32a3a6` |
| Vendor early firmware | BSP commit `25f5b88ec4ba34029f964693dc34028b26e6c67c`; retained DDR/FSBL |
| Installed SPI full-chip SHA256 | `f7ae1a4130351fa77f7828e65b549370ef221135a59a8153ef130429da8ba651`; CRC32 `845cc5f7` |

The secondary-CPU fix is already established: FSBL starts CPU0 at `0x100000`
and secondary CPUs at `0x30000`; SPI installs a verified 112-byte waiting loop
at the latter address. Linux includes the matching EL3-to-EL2 entry and
32-bit mailbox patches. Preserve this code during extraction. A repository
split does not require reopening the solved CPU investigation or replacing
the working kernel with an unmodified Fedora kernel.

Evidence:

- [Common-image host build](common-modes-build-2026-11-27.md): 55 Mock tests,
  actual RPM/payload checks and cached image reuse.
- [Common Server boot](hardware/common-server-2026-11-27.md): the operator
  confirmed release 3 and Server running/next mode status.
- [Latest SPI write](hardware/spi-modes-flash-2026-11-27.md): full 16 MiB
  readback and a cold boot with four CPUs, 1.9 GiB RAM and capability `v1`.
- [Earlier CPU-fix qualification](hardware/spi-cpu-flash-2026-11-26.md):
  repeated cold/warm boots, bounded memory/coherency and kernel fallback tests.
- [SW2=1 USB rescue observation](hardware/usb-rescue-2026-11-27.md): Realtek
  `0bda:1195` enumerates with a vendor-specific interface. The official Windows
  tool documents Android/eMMC flashing; SPI restoration through USB remains
  unqualified and belongs to the standalone SPI project's recovery work.
- [eMMC clock fix](hardware/emmc-clock-2026-11-27.md): accept the verified
  80 MHz firmware handoff; a module-only test restored the 7.28 GiB read-only
  device and passed repeated direct reads. The release-4 RPM was then built,
  installed and booted, with read-only eMMC present automatically. Preserve
  the patch and its native clock guard checks with the OS RPM sources.

Private durable SPI backups and the UART update bundle are under
`~/.local/share/bpi-m4/spi-backups/2026-11-27/modes-update/`; the portable archive
is `~/.local/share/bpi-m4/spi-backups/2026-11-27/bpi-m4-spi-2GB-20261127.tar.gz`.
See [SPI-UPDATE.md](SPI-UPDATE.md). `make clean-hard` deletes ignored files
inside the checkout, including `out/`, `work/` and `cache/`; preserve useful
products outside the checkout before cleanup.

Media packages/mode exist, but Media RAM/RPC/HDMI and switching are not yet
hardware qualified. Server is the default with full RAM; optional Media uses
a separate kernel/DTB and the same root/initramfs. Physical power cycling is
the supported switch procedure. Read-only eMMC is available in the installed
release-4 kernel; writable eMMC and direct eMMC boot remain unfinished. The
saved common image still contains release 3 and must be rebuilt to include
the fix. Kernel-devel packaging is requested below and remains to implement.

## Ownership

| Repository | Current inputs to move or retain | Products |
| --- | --- | --- |
| BPI-M4-COPR | `packaging/bpi-m4-kernel.spec`, `src/linux/`, `patches/linux/`, kernel configs and numeric release | Server and Media kernel RPMs with matching DTBs/modules; matching kernel-devel packages |
| BPI-M4-COPR | Board-support, sd-boot, codec and media-firmware SPECs | Board support, optional Media metapackage, boot support, codecs and firmware |
| BPI-M4-COPR | `src/initramfs/init-sd`, `sd-root.bash`, `bpi-m4-bootctl`, `bpi-m4-mode`, HDMI/player commands and packaged configuration/docs | Runtime commands and generic initramfs owned by RPMs |
| BPI-M4-COPR | RPM source preparation, initramfs/codec builders, rpkg/Mock configuration, source locks, licences and package tests | Self-contained SRPMs and OS RPMs built/published by COPR; local Mock checks for development |
| BPI-M4-IMAGE | `build-image`, `build-image-direct.bash`, `image-appliance-worker.bash`, `image-manifest.jq` | One Server-default raw SD image, manifest and checksum |
| BPI-M4-IMAGE | Image configuration, presets, Fedora/COPR package locks, provisioning/layout logic, `flash-sd.bash` and image/flash tests | Image customization and operator flash/readback; future eMMC installation |
| BPI-M4-SPI | `src/spi/`, active shared `src/u-boot/sd.c` and `cpus.c`, `patches/u-boot/` | Modern SPI U-Boot and raw boot-region candidate |
| BPI-M4-SPI | SPI SPECs, `build-spi*.bash`, packer/crypto/reference checks and pinned vendor inputs | SPI SRPM/RPM, layout and integrity receipts |
| BPI-M4-SPI | SPI RAM diagnostics, UART guide and SPI hardware records | Programming helpers, qualified release evidence and recovery instructions |

### Exact SPEC migration map

This table covers every current `packaging/*.spec`. Repository ownership does
not imply that every build-only or vendor artifact must be published in COPR.

| Current SPEC | Destination | Required treatment |
| --- | --- | --- |
| `bpi-m4-board-support.spec` | BPI-M4-COPR | Board configuration and runtime commands; also produces the optional Media metapackage |
| `bpi-m4-kernel.spec` | BPI-M4-COPR | Server/Media kernels, DTBs and modules; add the matching kernel-devel subpackages described below |
| `bpi-m4-sd-boot.spec` | BPI-M4-COPR | Generic initramfs and boot/mode selection commands; consumed by image assembly and kernel updates |
| `bpi-m4-native-codecs.spec` | BPI-M4-COPR | Optional Media player/codecs; qualify native aarch64 compilation |
| `bpi-m4-media-firmware.spec` | BPI-M4-COPR | Optional Media firmware; retain the existing redistribution/licence qualification before public publication |
| `bpi-m4-build-checks.spec` | BPI-M4-COPR, adapted | Keep only OS/RPM/kernel tests in this build-only test recipe; move image tests to BPI-M4-IMAGE and SPI tests to BPI-M4-SPI with independent runners |
| `bpi-m4-spi.spec` | BPI-M4-SPI | Standalone modern bootloader/firmware build; not an OS COPR or image-build dependency |
| `bpi-m4-spi-vendor.spec` | BPI-M4-SPI | Vendor updater/reference build for comparison and recovery research; retain current tools and qualification status |
| `bpi-m4-boot.spec` | BPI-M4-fc history | Legacy v2026.10 image-specific SD second-stage loader; do not migrate this package to the active OS workflow |

`BPI-M4-IMAGE` needs none of these SPECs: it installs published Fedora/COPR
RPMs and assembles storage images. The combined checks SPEC must not be copied
unchanged into any new repository, because it currently bundles all three
projects' tests. Each project must retain independent checks without requiring
another checkout. A build-only checks RPM is not installed into an SD image;
publishing that test artifact in COPR is optional.

For the non-SPEC entries in `packaging/`, `kernel-update-release` belongs to
`BPI-M4-COPR`. Split `files/` by the owning RPM rather than copying everything.
Give the COPR and SPI repositories their own `rpkg.conf` and relevant local
Mock configuration; the image repository requires neither. Kernel-devel
subpackages do not exist yet and should share the kernel SPEC/source build,
so their configuration, headers, symbols and ABI cannot drift from the kernel.

The generic initramfs belongs to the RPM project even though its package is
named `bpi-m4-sd-boot`: installed kernels use it for updates and mode selection.
Image assembly consumes its RPM. Individual SSH keys, hostname, user creation
and root partition UUID belong to image provisioning.

Keep the old v2026.10 SD second-stage workflow and its image-specific boot
package in the tagged historical repository. Extract active common helpers
before retiring that code; for example, generic initramfs assembly currently
imports RPM extraction from `bpi_image/boot.py`. Historical files are not all
dead code, and active SPI SD/CPU sources must be retained.

Upstream kernel/U-Boot archives and generated RPMs/images do not belong in Git.
Keep patches, build inputs, pinned source locations/hashes and provenance;
source preparation produces self-contained SRPMs. Full SPI chip dumps remain
private board-specific backups outside cleanup paths.

## Matching kernel development packages

The user requested this addition on 2026-11-27: `BPI-M4-COPR` should produce
an optional `bpi-m4-kernel-devel` package alongside each Server kernel, and a
matching Media development package when Media is offered. These packages
have not been implemented in the historical repository. They are optional
development tools, not dependencies of the default SD image or SPI build.

Build them from the same patched source, configuration and numeric/recipe
release as their kernel. Provide the prepared external-module build tree
under `/usr/src/kernels/RELEASE` and the conventional
`/usr/lib/modules/RELEASE/build` link. Include Kbuild/Makefiles, public and
generated ARM64 headers, `.config`, `Module.symvers`, and required build
scripts/helpers. Board-driver tests also need the internal headers they use;
the current eMMC driver includes `drivers/mmc/core/card.h`. Include that
dependency or adapt the test build to a supported interface.

Each package must require its exact `kernel-uname-r = RELEASE` and provide
`kernel-devel-uname-r = RELEASE`. Keep different ABI versions installable
side by side, including the working fallback. Fedora's generic kernel-devel
cannot substitute for this board kernel's patched source and configuration.
Native aarch64 build helpers must run on the board: the current cross build
produces some x86_64 host tools, so copying its prepared tree unchanged is not
sufficient for an aarch64 development RPM. Qualify the native COPR package
and, if retained, a separate documented cross-build use case.

The intended operator workflow is:

```sh
dnf install "kernel-devel-uname-r = $(uname -r)"
make -C "/usr/lib/modules/$(uname -r)/build" M="$PWD" modules
```

The operator runs privileged installation manually or in an authorized root
board shell. A native compiler/toolchain is also required. This permits small
driver or diagnostic modules to compile on the board without downloading,
preparing or compiling the whole kernel. Modules still need hardware testing;
header availability does not qualify an eMMC write path. The eMMC clock
investigation above is the concrete smoke-test use case for this package.

## Dependencies found during review

1. `make image` invokes OS RPM builds and SPI builds through one Makefile.
   The image repository should download completed OS RPMs from Fedora/COPR.
   Its normal build should not require SPI artifacts.
2. `build-image-direct.bash` and `image-manifest.jq` require local
   `out/rpms/*.json` receipts and `out/spi/build-audit.json`. Replace these
   checkout-specific requirements with a release input lock and installed
   RPM metadata. Preserve package, payload and checksum checks.
3. Image assembly currently requires a SPI build to copy `u-boot-spi.bin` onto
   FAT. That file is a RAM-test/recovery artifact; normal boot loads Linux
   directly from the SD root. Remove this prerequisite from normal image
   assembly. Recovery tools and bootloader binaries belong to the SPI project.
4. `inputs.server.lock.json` combines kernel source pins and Fedora runtime
   packages. Separate kernel/source locks, initramfs runtime inputs and image
   package locks so refreshing userspace does not change kernel sources.
5. `bpi_image/rpm_packages.py` combines all source preparation and depends on
   shared image/source helpers. Extract only what each project needs; retain
   tested Python preparation initially where replacing it would change the
   build. The image installer is already predominantly Bash and jq.
6. SPI preparation archives the entire `patches/` directory and watches broad
   source trees. Restrict it to U-Boot inputs so Linux patches cannot invalidate
   SPI builds.
7. The regression SRPM currently packages almost the whole repository.
   Give each project its own tests/fixtures, with integration checks using
   released artifacts rather than another repository's source tree.

## Interface between projects

The image builder downloads OS RPMs from Fedora and the selected COPR project.
Record exact package identities, hashes, origin and kernel release in its input
lock and output manifest. Preserve Fedora signature checks and verify COPR
packages with the corresponding repository key. An image-only build must run
without Mock, compilers, SPI artifacts or either source checkout. Keep the
existing rootless libguestfs installation and manual verified flashing.

SPI publishes U-Boot/boot-region files, hashes, RAM size, layout and a hardware
qualification record. Its runtime interface remains SD extlinux plus the DTB
profile/memory contract and `bananapi,spi-media-handoff = "v1"` capability.
Kernel updates stay independent of SPI releases. Installation images document
the compatible SPI prerequisite; firmware programming remains a separate task.

Small rpkg/Mock wrappers can live in both source repositories. A fourth shared
tools project or mandatory Git submodules would add coordination without
solving the current coupling.

## Standalone SPI build

The current SPI script already fetches upstream U-Boot and the pinned vendor
BSP, prepares a self-contained SRPM and compiles it in Mock. It does not need
a Linux build or SD image. It is not yet an independently extracted repository:
it uses helpers/configuration from this checkout and copies broad source trees.

The new SPI repository owns:

- All active U-Boot board code, including the currently shared `sd.c` and
  `cpus.c`, its patches and board configuration.
- Its own source-fetching, SRPM/Mock wrappers, toolchain receipt and tests.
  Replace the current Makefile's Python OS-build toolchain helper with a
  SPI-local tool/version check.
- Pinned vendor DDR/FSBL inputs, native packer/crypto tools, layout checks,
  diagnostics, backup/update documentation and qualification records.

A fresh SPI checkout plus ordinary host dependencies and upstream downloads
must be sufficient to build it. Neither COPR nor the image repository is a
build dependency. The SPI RPM is its internal packaging artifact; OS image
assembly does not install it. Raw boot-region artifacts and board-specific
full-chip backups retain their distinct meanings.

At runtime, the loader reads extlinux and compatible kernel/DTB/initramfs
files from storage. That documented compatibility contract is shared across
independently released projects; no kernel version is compiled into SPI.

## Future eMMC support

The image repository owns eMMC image layout and installation. Actual eMMC
boot additionally requires qualified SPI eMMC reading and Linux eMMC support.
The saved release-3 common image reported `-22` with `Unverified inherited
eMMC clock`. A subsequent [clock handoff fix](hardware/emmc-clock-2026-11-27.md)
uses the verified vendor 80 MHz rate instead of requiring 200 MHz; read-only
enumeration and bounded reads passed in the module test, and the installed
release-4 kernel booted with eMMC present. Preserve
`patches/linux/50-emmc-clock-handoff.patch` with the kernel SPEC and
`tests/check-emmc-clock.bash` / `tests/emmc-clock-harness.c` in `BPI-M4-COPR`.

Booting from eMMC still needs a qualified writable Linux driver, SPI eMMC
reading, and an initramfs/root-write policy that accepts eMMC. The current
SPI reader and initramfs are SD-specific. Cloning the SD partitions alone
does not add these capabilities, and SW2=1 selects the vendor download/rescue
path rather than a Fedora partition layout. A future copy must cover the
complete image (currently 2,416,967,680 bytes), use an offline source, and
account for duplicate partition identifiers while both devices are attached.
These are future hardware changes, separate from repository extraction. eMMC
support spans the boot/runtime contract, while the three source/build
repositories remain independent.

## Migration sequence and checks

1. Preserve `BPI-M4-fc` and the working revision, built RPMs/image and private
   SPI backups as the historical baseline.
2. Extract OS packaging into `BPI-M4-COPR`. Rebuild self-contained SRPMs in
   fresh Mock roots and verify RPM dependencies, kernel ABI/modules, mode
   selection, installation/removal and fallback. Add matching kernel-devel
   packages and verify a small external-module build against the packaged
   tree. Source-preparation moves can
   change recipe hashes; new kernel releases need the normal numeric release
   increment and must retain the working fallback.
3. Qualify native COPR builds and publish the required OS packages. Create
   `BPI-M4-IMAGE` to consume those packages directly through Fedora/COPR,
   with an image-input lock. Verify package inventory, boot bundles and
   flash readback logic on a host without Mock or source trees.
4. Extract SPI source, active shared SD/CPU code and packaging into
   `BPI-M4-SPI`. Verify reference packer comparisons, retained DDR/FSBL bytes,
   output layout and secondary-entry checks independently of OS packaging.
5. Keep the current boot contract and disk layout for the first migration.
   Confirm a Server boot of the new image and independent SPI build checks.
   Leave `BPI-M4-fc` available for history and the known working baseline.

Repository separation and COPR publication are separate milestones. The kernel
SPEC already selects native tools on aarch64, but its native COPR build has
not run. The generic boot and codec builds still use explicit cross tools and
need native-path work before the complete package set is offered through COPR.
Vendor firmware redistribution remains subject to the existing licence review;
local packaging can be extracted without treating it as publicly distributable.

After separation, kernel releases are built once in the RPM project. Image
assembly installs existing RPMs; SPI is rebuilt only for bootloader changes.

## Completion criteria

- `BPI-M4-COPR` prepares self-contained SRPMs and passes package tests from a
  fresh checkout. Required Server packages have successful native
  `fedora-45-aarch64` COPR builds and an identified repository/key. Package
  installation, matching modules, boot activation, update/fallback and removal
  retain the existing behavior. Optional kernel-devel packages match their
  kernel ABI, retain side-by-side versions, and compile a small module using
  the installed aarch64 build tree without a full kernel rebuild. Optional
  Media sources retain their licensing
  status and need their own native/publication qualification.
- `BPI-M4-IMAGE` builds the Server-default SD image using only its checkout,
  ordinary image tools, Fedora repositories and the selected COPR repository.
  It requires no Mock, compiler, OS source tree, SPI binary or sibling checkout.
  Its manifest records exact RPM versions/origins/hashes; package inventory,
  root/boot configuration and image checksum pass. Operator flashing retains
  direct readback verification. A subsequent Server hardware boot is recorded.
- `BPI-M4-SPI` builds and checks its U-Boot/firmware artifacts from only its
  checkout, standard build dependencies and pinned upstream/vendor sources.
  Retained DDR/FSBL comparisons, layout/crypto checks, SD boot and secondary
  entry tests pass without the other repositories. Published artifacts clearly
  distinguish raw build regions from private full-chip images. Migration alone
  does not establish new hardware qualification for changed firmware.
- `BPI-M4-fc` remains the historical project with its working implementation
  and evidence. Each new repository documents the boot contract and its own
  independent build/release process.

Suggested next-session task:

```text
Implement docs/REPOSITORY-SPLIT.md. Keep BPI-M4-fc as history and create
independent BPI-M4-COPR, BPI-M4-IMAGE and BPI-M4-SPI projects. Follow the
documented migration sequence, established hardware baseline and AGENTS.md.
```
