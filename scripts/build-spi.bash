#!/usr/bin/bash
# Prepare pinned sources with native tools; all compilation happens in Mock.
set -Eeuo pipefail
export LC_ALL=C
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
cd "$repo"
mode=${1:-build} jobs=${2:-8} dram=${3:-2}
[[ $mode == build || $mode == srpm ]]
[[ $jobs =~ ^[0-9]+$ ]] && (( jobs >= 1 && jobs <= 64 ))
[[ $dram == 1 || $dram == 2 ]] || { echo 'SPI_DRAM_GIB must be 1 or 2.' >&2; exit 2; }
[[ $EUID != 0 ]] || { echo 'Run as your regular user.' >&2; exit 1; }
uboot_revision=ee1b3477e3af6af1ae9b8f98bc84d2cc6f32a3a6
vendor_revision=25f5b88ec4ba34029f964693dc34028b26e6c67c
work=work/rpms/spi
mkdir -p cache/spi out/srpms out/spi
./scripts/clean-rpm-work.bash "$work"
mkdir -p "$work/SOURCES" "$work/srpm-result" "$work/result"
archive_git() {
    local url=$1 checkout=$2 revision=$3 prefix=$4 output=$5
    shift 5
    if [[ ! -d $checkout/.git ]]; then
        git clone --depth=1 --filter=blob:none --no-checkout "$url" "$checkout"
    fi
    if ! GIT_NO_LAZY_FETCH=1 git -C "$checkout" cat-file -e "$revision^{commit}"; then
        git -C "$checkout" fetch --depth=1 origin "$revision"
    fi
    [[ $(git -C "$checkout" rev-parse "$revision^{commit}") == "$revision" ]]
    git -C "$checkout" archive --format=tar --prefix="$prefix/" "$revision" "$@" >"$output.tar"
}
archive_git https://github.com/u-boot/u-boot.git cache/spi/u-boot/git \
    "$uboot_revision" u-boot-v2027.01-rc3 "$work/SOURCES/upstream"
bzip2 -c "$work/SOURCES/upstream.tar" >"$work/SOURCES/u-boot-v2027.01-rc3.tar.bz2"
archive_git https://github.com/BPI-SINOVOIP/BPI-M4-bsp.git cache/spi/bpi-m4-bsp \
    "$vendor_revision" bpi-m4-spi-vendor-2015.07 "$work/SOURCES/vendor" u-boot-rtk
gzip -n -c "$work/SOURCES/vendor.tar" >"$work/SOURCES/bpi-m4-spi-vendor-2015.07.tar.gz"
rm -- "$work/SOURCES/upstream.tar" "$work/SOURCES/vendor.tar"
mkdir -p "$work/port/bpi-m4-spi-port/src" "$work/port/bpi-m4-spi-port/packaging/files" \
    "$work/port/bpi-m4-spi-port/scripts"
cp -a src/spi src/u-boot "$work/port/bpi-m4-spi-port/src/"
mkdir -p "$work/port/bpi-m4-spi-port/patches"
cp -a patches/u-boot "$work/port/bpi-m4-spi-port/patches/"
cp -a packaging/files/extlinux.conf.example "$work/port/bpi-m4-spi-port/packaging/files/"
cp -a scripts/{pack-spi,check-spi-tools,spi-crypto}.bash "$work/port/bpi-m4-spi-port/scripts/"
tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner \
    -C "$work/port" -cf - bpi-m4-spi-port | gzip -n >"$work/SOURCES/bpi-m4-spi-port.tar.gz"
(cd "$work/SOURCES"; sha256sum u-boot-v2027.01-rc3.tar.bz2 \
    bpi-m4-spi-vendor-2015.07.tar.gz bpi-m4-spi-port.tar.gz >spi-sources.sha256)
printf '%s\n' "$dram" >"$work/SOURCES/spi-dram-gib"
build_id=$({
    printf '%s\n' "$uboot_revision" "$vendor_revision" "$dram"
    sha256sum packaging/bpi-m4-spi.spec scripts/build-spi.bash \
        scripts/build-srpm.bash scripts/build-rpm.bash packaging/rpkg.conf \
        packaging/mock/bpi-m4-f45-cross.cfg
    cat "$work/SOURCES/spi-sources.sha256"
} | sha256sum)
build_id=${build_id:0:12}
./scripts/build-srpm.bash "$repo/packaging/rpkg.conf" "$repo/packaging/bpi-m4-spi.spec" \
    "$repo/$work" "$build_id" server v2027.01
srpms=("$work/srpm-result/"*.src.rpm)
(( ${#srpms[@]} == 1 ))
cp -- "${srpms[0]}" out/srpms/spi.src.rpm
(cd out/srpms; sha256sum spi.src.rpm >spi.src.rpm.sha256)
[[ $mode != srpm ]] || exit 0
echo "Building SPI U-Boot v2027.01-rc3; logs: $work/result/build.log"
./scripts/build-rpm.bash "$repo/packaging/mock/bpi-m4-f45-cross.cfg" \
    "$repo/out/srpms/spi.src.rpm" "$repo/$work" "bpi-m4-spi-$build_id" "$jobs"
rpms=("$work/result/"*.aarch64.rpm)
(( ${#rpms[@]} == 1 ))
cp -- "${rpms[0]}" out/spi/bpi-m4-spi.rpm
(cd out/spi; sha256sum bpi-m4-spi.rpm >bpi-m4-spi.rpm.sha256)
mkdir -p "$work/audit"
(cd "$work/audit"; rpm2cpio "$repo/out/spi/bpi-m4-spi.rpm" | cpio --quiet -idm --no-absolute-filenames)
cp -a "$work/audit/usr/lib/bpi-m4/spi-${dram}GB/." out/spi/
cp -- "$work/result/installed_pkgs.log" out/spi/installed_pkgs.log
jq -n --arg uboot_revision "$uboot_revision" --arg vendor_revision "$vendor_revision" \
    --argjson dram "$dram" --slurpfile layout out/spi/spi-layout.json \
    --rawfile packages out/spi/installed_pkgs.log \
    --arg srpm_sha256 "$(cut -d ' ' -f 1 out/srpms/spi.src.rpm.sha256)" \
    --arg rpm_sha256 "$(cut -d ' ' -f 1 out/spi/bpi-m4-spi.rpm.sha256)" \
    '{uboot:{version:"v2027.01-rc3",revision:$uboot_revision},
      vendor:{revision:$vendor_revision},dram_gib:$dram,
      srpm_sha256:$srpm_sha256,rpm_sha256:$rpm_sha256,
      build_tools:($packages|split("\n")|map(split(" ")[0] // "")|
        map(select(test("^(gcc-|gcc-aarch64-linux-gnu-|binutils-|binutils-aarch64-linux-gnu-|openssl-|openssl-devel-)")))),
      layout:$layout[0],hardware_flashed:false,hardware_validated:false}' >out/spi/build-audit.json
echo 'SPI candidate RPM built; no physical device was accessed.'
