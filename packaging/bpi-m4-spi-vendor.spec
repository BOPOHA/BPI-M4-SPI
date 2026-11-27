%global debug_package %{nil}
%global __os_install_post %{nil}
%global _build_id_links none
%global _buildshell /bin/bash
Name: bpi-m4-spi-vendor
Version: 2015.07
Source99: bpi-build-id
%global bpi_recipe %(cat %{SOURCE99})
Release: 1.%{bpi_recipe}.fc45
Summary: Local RTD1395 vendor SPI updater build baseline
License: GPL-2.0-or-later AND LicenseRef-Realtek-Firmware
URL: https://github.com/BPI-SINOVOIP/BPI-M4-bsp
Source0: bpi-m4-spi-vendor-2015.07.tar.gz
Source1: spi-source.sha256
Source97: spi-dram-gib
%global bpi_dram_gib %(cat %{SOURCE97})
BuildRequires: gcc-aarch64-linux-gnu
BuildRequires: binutils-aarch64-linux-gnu
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: bison
BuildRequires: flex
BuildRequires: openssl-devel
BuildRequires: openssl
BuildRequires: perl
BuildRequires: dtc
BuildRequires: file
BuildRequires: binutils
AutoReqProv: no

%description
Tests the unchanged pinned vendor source with current Fedora build tools.
No bundled GCC or obsolete OpenSSL runtime is used. The generated program is
an in-system SPI updater, not a raw flash dump. This local research package
does not run the updater or install anything into SPI. Vendor firmware
redistribution rights and hardware qualification require separate review.

%prep
(cd %{_sourcedir}; sha256sum --check --strict %{SOURCE1})
%setup -q

%build
unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS
cd u-boot-rtk
gcc --version
aarch64-linux-gnu-gcc --version
openssl version
# flash_writer_vm/config.mk uses these historical names. Map them to the
# current Fedora compiler without modifying the vendor source or using an SDK.
mkdir -p native-tools
for tool in gcc ld objcopy objdump; do
    ln -s "$(command -v aarch64-linux-gnu-$tool)" "native-tools/aarch64-linux-$tool"
done
export PATH="$PWD/native-tools:$PATH"
# Probe host crypto independently so a U-Boot failure cannot hide its status.
set +e
make -C examples/flash_writer_vm/tools/RtSSL LIB_PATH=/usr/lib64 STATIC_LINK=N \
    >rtssl-build.log 2>&1
crypto_status=$?
make CROSS_COMPILE=aarch64-linux-gnu- rtd1395_spi_bananapi_defconfig \
    >vendor-build.log 2>&1
vendor_status=$?
if (( vendor_status == 0 )); then
    make -j%{_smp_build_ncpus} CROSS_COMPILE=aarch64-linux-gnu- \
        Board_HWSETTING=RTD1395_hwsetting_BOOT_%{bpi_dram_gib}DDR4_8Gb_s2133 \
        >>vendor-build.log 2>&1
    vendor_status=$?
fi
set -e
printf 'Current-toolchain results: RtSSL=%s vendor-SPI=%s\n' "$crypto_status" "$vendor_status"
tail -n 50 rtssl-build.log
tail -n 70 vendor-build.log
(( crypto_status == 0 && vendor_status == 0 )) || {
    echo 'Unchanged vendor SPI sources failed with current Fedora tools; no legacy-toolchain fallback.' >&2
    exit 1
}

%install
dest="%{buildroot}/usr/lib/bpi-m4/spi-vendor"
mkdir -p "$dest"
install -pm0644 u-boot-rtk/examples/flash_writer_vm/dvrboot.exe.bin \
    "$dest/spiloader-%{bpi_dram_gib}GB.bin"
install -pm0644 u-boot-rtk/u-boot.bin u-boot-rtk/u-boot "$dest/"
install -pm0644 u-boot-rtk/.config "$dest/vendor.config"
install -pm0644 u-boot-rtk/{rtssl-build.log,vendor-build.log} "$dest/"
install -Dpm0644 u-boot-rtk/Licenses/gpl-2.0.txt \
    "%{buildroot}/usr/share/licenses/%{name}/gpl-2.0.txt"

%check
dest="%{buildroot}/usr/lib/bpi-m4/spi-vendor"
readelf -h "$dest/u-boot" | grep -Eq 'Machine:.*AArch64'
readelf -h "$dest/u-boot" | grep -Eq 'Entry point address:.*0x100000'
test -s "$dest/spiloader-%{bpi_dram_gib}GB.bin"
test "$(stat -c %%s "$dest/spiloader-%{bpi_dram_gib}GB.bin")" -le "$((16 * 1024 * 1024))"
test "$(stat -c %%s "$dest/u-boot.bin")" -gt 0
test "$(stat -c %%s "$dest/u-boot.bin")" -le "$((0xC0000))"
grep -Eq '^CONFIG_SYS_TEXT_BASE=(0x)?0*100000$' u-boot-rtk/include/autoconf.mk
(cd "$dest"; sha256sum spiloader-%{bpi_dram_gib}GB.bin u-boot.bin >SHA256SUMS)

%files
%license /usr/share/licenses/%{name}
/usr/lib/bpi-m4/spi-vendor
