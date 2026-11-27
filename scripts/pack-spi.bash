#!/usr/bin/bash
# Build a local SPI boot-region candidate. Never execute flash_writer_vm.
set -Eeuo pipefail
export LC_ALL=C
vendor=$(realpath "$1") uboot=$(realpath "$2") dram=$3
tools=$(realpath "$4") layout=$(realpath "$5") result=$(realpath "$6")
[[ $dram == 1 || $dram == 2 ]]
[[ $(stat -c %s "$uboot") -le $((0xC0000)) ]]
mkdir -p "$result/Bind"
cd "$result"
hwsetting="$vendor/image/hw_setting/rtd1395/bananapi/0001/RTD1395_hwsetting_BOOT_${dram}DDR4_8Gb_s2133.config"
# The retained vendor generator is rebuilt with the current host GCC.
gcc -O2 -std=gnu11 "$vendor/tools/hw_setting_maker/generate_config.c" -o generate-config
./generate-config "$hwsetting" hwsetting.raw >hwsetting.log
# Compare its bytes with the pinned host reference, without an old toolchain.
"$vendor/tools/hw_setting_maker/generate_config" "$hwsetting" hwsetting.reference >>hwsetting.log
cmp hwsetting.raw hwsetting.reference
"$tools" pad hwsetting.raw Bind/hwsetting_padding.bin hwsetting.digest
cat hwsetting.raw Bind/hwsetting_padding.bin >hwsetting.padded
"$tools" obfuscate "$vendor/image/aes_module_id.bin" "$vendor/image/aes_sck_flash.bin" \
    "$vendor/image/aes_vendor_id.bin" hwsetting.padded hwsetting.encrypted
"$tools" sign "$vendor/image/rsa_key_2048.pem" hwsetting.digest hwsetting.signature
cat hwsetting.encrypted hwsetting.signature >Bind/hwsetting.bin
"$tools" pad "$uboot" Bind/uboot_padding.bin uboot.digest
cat "$uboot" Bind/uboot_padding.bin uboot.digest >Bind/uboot.bin
cp "$vendor/bootimage/rtd1395/0001/bootmon-new.bin" Bind/arm_romcode.bin
cp "$vendor/bootimage/rtd1395/0001/mips-bootmon-new.bin" Bind/mips_romcode.bin
cp "$vendor/bootimage/rtd1395/0001/fsbl-loader-spi-00.00-bootcode-addr-0x00100000.bin.enc" Bind/fsbl.bin
"$layout" header Bind/hwsetting.bin Bind/uboot.bin Bind/fsbl.bin Bind/hwsetting_header.bin
# The active vendor recipe always supplies boot_enc to the header/packer.
# Preserve that convention and compare the complete boot region independently.
gcc -O2 -std=gnu89 -Wno-implicit-int -Wno-implicit-function-declaration \
    -include string.h \
    "$vendor/tools/bindAllBins/bindAllBins.c" -o bind-reference
./bind-reference fsbl secure header boot_enc >layout-reference.log
"$layout" header Bind/hwsetting.bin Bind/uboot.bin Bind/fsbl.bin header.reference
cmp Bind/hwsetting_header.bin header.reference
openssl dgst -sha256 -binary Bind/hwsetting_header.bin >header.digest
cat Bind/hwsetting_header.bin header.digest >header.plain
"$tools" obfuscate "$vendor/image/aes_module_id.bin" "$vendor/image/aes_sck_flash.bin" \
    "$vendor/image/aes_vendor_id.bin" header.plain Bind/hwsetting_header_enc.bin
./bind-reference fsbl secure boot_enc >>layout-reference.log
"$layout" assemble Bind/arm_romcode.bin Bind/mips_romcode.bin Bind/hwsetting_header_enc.bin \
    Bind/hwsetting.bin Bind/uboot.bin Bind/fsbl.bin spi-boot-region.bin spi-layout.json
cmp Bind/bind.bin spi-boot-region.bin
printf 'RTD1395 DDR generation, header and complete boot-region reference comparisons passed.\n'
sha256sum spi-boot-region.bin >spi-boot-region.bin.sha256
