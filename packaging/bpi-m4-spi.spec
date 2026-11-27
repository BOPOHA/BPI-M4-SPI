%global debug_package %{nil}
%global __os_install_post %{nil}
%global _build_id_links none
%global _buildshell /bin/bash
Name: bpi-m4-spi
Version: v2027.01
Source99: bpi-build-id
%global bpi_recipe %(cat %{SOURCE99})
Release: 0.3.rc3.%{bpi_recipe}.fc45
Summary: Modern RTD1395 SPI loader with generic SD extlinux boot
License: GPL-2.0-or-later AND LicenseRef-Realtek-Firmware
URL: https://git.u-boot-project.org/u-boot/u-boot
Source0: u-boot-v2027.01-rc3.tar.bz2
Source1: bpi-m4-spi-vendor-2015.07.tar.gz
Source2: bpi-m4-spi-port.tar.gz
Source3: spi-sources.sha256
Source97: spi-dram-gib
%global bpi_dram_gib %(cat %{SOURCE97})
BuildRequires: gcc-aarch64-linux-gnu
BuildRequires: binutils-aarch64-linux-gnu
BuildRequires: gcc
BuildRequires: make
BuildRequires: bison
BuildRequires: flex
BuildRequires: python3
BuildRequires: python3-setuptools
BuildRequires: openssl-devel
BuildRequires: openssl
BuildRequires: perl
BuildRequires: perl(Math::BigInt)
BuildRequires: perl(bigint)
BuildRequires: patch
BuildRequires: dtc
BuildRequires: file
BuildRequires: jq
AutoReqProv: no

%description
Build-only SPI firmware candidate using current Fedora cross tools. Retains
the pinned vendor early firmware and DDR data. Versioned kernels, initramfs
and DTBs are selected from SD using upstream extlinux configuration parsing.
Installing this local research package does not program SPI or an SD card.

%prep
(cd %{_sourcedir}; sha256sum --check --strict %{SOURCE3})
%setup -q -n u-boot-v2027.01-rc3 -a 1 -a 2
inputs=bpi-m4-spi-port
patch -p1 <"$inputs/patches/u-boot/0001-register-bpi-m4-board.patch"
patch -p1 <"$inputs/patches/u-boot/0002-use-32-bit-secondary-core-mailbox.patch"
for file in Kconfig Makefile board.c spi-boot.c lowlevel.S sd-init.c secondary.S; do
    install -Dpm0644 "$inputs/src/spi/$file" "board/bananapi/bpi-m4-fc/$file"
done
for file in sd.c cpus.c; do
    install -Dpm0644 "$inputs/src/u-boot/$file" "board/bananapi/bpi-m4-fc/$file"
done
install -Dpm0644 "$inputs/src/spi/bpi-m4-fc.h" include/configs/bpi-m4-fc.h
install -Dpm0644 "$inputs/src/spi/board.dts" arch/arm/dts/bpi-m4-fc.dts
install -Dpm0644 "$inputs/src/spi/defconfig" configs/bpi_m4_spi_defconfig

%build
unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS
aarch64-linux-gnu-gcc --version
gcc --version
openssl version
make CROSS_COMPILE=aarch64-linux-gnu- bpi_m4_spi_defconfig
%if %{bpi_dram_gib} == 1
scripts/config --set-val BPI_M4_RAM_SIZE 0x40000000
make CROSS_COMPILE=aarch64-linux-gnu- olddefconfig
%endif
for option in CONFIG_BPI_M4_SPI=y CONFIG_CMD_SYSBOOT=y CONFIG_FS_EXT4=y CONFIG_EFI_PARTITION=y CONFIG_ENV_IS_NOWHERE=y CONFIG_OF_BOARD_SETUP=y; do
    grep -Fx "$option" .config
done
%make_build CROSS_COMPILE=aarch64-linux-gnu- u-boot.bin
gcc -O2 -std=gnu11 -Wall -Wextra -Werror bpi-m4-spi-port/src/spi/spi-tools.c \
    -lcrypto -o spi-tools
gcc -O2 -std=gnu11 -Wall -Wextra -Werror bpi-m4-spi-port/src/spi/spi-layout.c \
    -lcrypto -o spi-layout
vendor="$PWD/bpi-m4-spi-vendor-2015.07/u-boot-rtk/examples/flash_writer_vm"
./bpi-m4-spi-port/scripts/check-spi-tools.bash "$PWD/spi-tools" "$vendor" \
    "$PWD/bpi-m4-spi-port/scripts/spi-crypto.bash"
mkdir -p spi-candidate
./bpi-m4-spi-port/scripts/pack-spi.bash "$vendor" "$PWD/u-boot.bin" \
    %{bpi_dram_gib} "$PWD/spi-tools" "$PWD/spi-layout" "$PWD/spi-candidate"

%install
dest="%{buildroot}/usr/lib/bpi-m4/spi-%{bpi_dram_gib}GB"
mkdir -p "$dest"
install -pm0644 u-boot.bin u-boot "$dest/"
install -pm0644 .config "$dest/uboot.config"
install -pm0644 spi-candidate/{spi-boot-region.bin,spi-boot-region.bin.sha256,spi-layout.json} "$dest/"
install -pm0644 %{SOURCE3} "$dest/spi-sources.sha256"
install -Dpm0644 bpi-m4-spi-port/packaging/files/extlinux.conf.example \
    "%{buildroot}/usr/share/doc/%{name}/extlinux.conf.example"
install -Dpm0644 Licenses/README "%{buildroot}/usr/share/licenses/%{name}/README"
cp -p Licenses/* "%{buildroot}/usr/share/licenses/%{name}/"

%check
readelf -h u-boot | grep -Eq 'Machine:.*AArch64'
readelf -h u-boot | grep -Eq 'Entry point address:.*0x100000'
test "$(stat -c %%s u-boot.bin)" -le "$((0xC0000))"
grep -aFq 'U-Boot 2027.01-rc3' u-boot.bin
grep -aFq 'BPI_M4_SPI_BOOT' u-boot.bin
! grep -aFq 'BPI_M4_FC_BOOT' u-boot.bin
grep -Fx 'CONFIG_GICV2=y' .config
# The secondary loop is copied verbatim to the FSBL's fixed RAM branch target.
# It must fit below Linux RAM, contain no text relocations, and read 32 bits.
pen_start=$(aarch64-linux-gnu-nm u-boot | awk '$3 == "bpi_m4_secondary_pen" {print $1}')
pen_end=$(aarch64-linux-gnu-nm u-boot | awk '$3 == "bpi_m4_secondary_pen_end" {print $1}')
test -n "$pen_start" && test -n "$pen_end"
test "$((0x$pen_end - 0x$pen_start))" -gt 0
test "$((0x$pen_end - 0x$pen_start))" -le "$((0x1000))"
aarch64-linux-gnu-readelf -r board/bananapi/bpi-m4-fc/secondary.o >secondary-relocations.log
! grep -Fq "'.rela.text'" secondary-relocations.log
aarch64-linux-gnu-objdump -d --disassemble=bpi_m4_secondary_pen u-boot >secondary-disassembly.log
grep -Eq 'ldr[[:space:]]+w0,[[:space:]]*\[x16\]' secondary-disassembly.log
test -s spi-candidate/spi-boot-region.bin
test "$(stat -c %%s spi-candidate/spi-boot-region.bin)" -le "$((0x200000))"
cmp bpi-m4-spi-vendor-2015.07/u-boot-rtk/examples/flash_writer_vm/bootimage/rtd1395/0001/fsbl-loader-spi-00.00-bootcode-addr-0x00100000.bin.enc spi-candidate/Bind/fsbl.bin
(cd "%{buildroot}/usr/lib/bpi-m4/spi-%{bpi_dram_gib}GB"; sha256sum u-boot.bin spi-boot-region.bin >SHA256SUMS)

%files
/usr/lib/bpi-m4/spi-%{bpi_dram_gib}GB
%doc /usr/share/doc/%{name}
%license /usr/share/licenses/%{name}
