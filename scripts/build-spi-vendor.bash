#!/usr/bin/bash
# Build-only vendor SPI baseline. Never execute the generated updater.
set -Eeuo pipefail
export LC_ALL=C
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
cd "$repo"
mode=${1:-build}
jobs=${2:-8}
dram=${3:-2}
[[ $mode == build || $mode == srpm ]] || { echo 'Mode must be build or srpm.' >&2; exit 2; }
[[ $jobs =~ ^[0-9]+$ ]] && (( jobs >= 1 && jobs <= 64 ))
[[ $dram == 1 || $dram == 2 ]] || { echo 'SPI_DRAM_GIB must be 1 or 2.' >&2; exit 2; }
[[ $EUID != 0 ]] || { echo 'Run as your regular user.' >&2; exit 1; }
revision=25f5b88ec4ba34029f964693dc34028b26e6c67c
url=https://github.com/BPI-SINOVOIP/BPI-M4-bsp.git
checkout=cache/spi/bpi-m4-bsp
work=work/rpms/spi-vendor
spec=packaging/bpi-m4-spi-vendor.spec
archive=bpi-m4-spi-vendor-2015.07.tar.gz
mkdir -p cache/spi "$work/SOURCES" out/srpms
./scripts/clean-rpm-work.bash "$work/srpm-result"
mkdir -p "$work/srpm-result"
if [[ ! -d $checkout/.git ]]; then
    git clone --depth=1 --filter=blob:none --no-checkout "$url" "$checkout"
fi
if ! git -C "$checkout" cat-file -e "$revision^{commit}"; then
    git -C "$checkout" fetch --depth=1 origin "$revision"
fi
[[ $(git -C "$checkout" rev-parse "$revision^{commit}") == "$revision" ]]
# Archive the Git object, ignoring working-tree changes and excluding toolchains.
git -C "$checkout" archive --format=tar --prefix=bpi-m4-spi-vendor-2015.07/ \
    "$revision" u-boot-rtk | gzip -n >"$work/SOURCES/$archive"
(cd "$work/SOURCES"; sha256sum "$archive" >spi-source.sha256)
printf '%s\n' "$dram" >"$work/SOURCES/spi-dram-gib"
build_id=$({
    printf '%s\n' "$revision" "$dram"
    sha256sum "$spec" scripts/build-spi-vendor.bash scripts/build-srpm.bash \
        scripts/build-rpm.bash packaging/rpkg.conf packaging/mock/bpi-m4-f45-cross.cfg
    cat "$work/SOURCES/spi-source.sha256"
} | sha256sum)
build_id=${build_id:0:12}
./scripts/build-srpm.bash "$repo/packaging/rpkg.conf" "$repo/$spec" \
    "$repo/$work" "$build_id" server 2015.07
srpms=("$work/srpm-result/"*.src.rpm)
(( ${#srpms[@]} == 1 ))
cp -- "${srpms[0]}" out/srpms/spi-vendor.src.rpm
(cd out/srpms; sha256sum spi-vendor.src.rpm >spi-vendor.src.rpm.sha256)
jq -n --arg revision "$revision" --arg url "$url" --argjson dram "$dram" \
    --arg source_sha256 "$(cut -d ' ' -f 1 "$work/SOURCES/spi-source.sha256")" \
    --arg srpm_sha256 "$(cut -d ' ' -f 1 out/srpms/spi-vendor.src.rpm.sha256)" \
    '{source:{url:$url,revision:$revision,sha256:$source_sha256},
      srpm:{path:"out/srpms/spi-vendor.src.rpm",sha256:$srpm_sha256},dram_gib:$dram,
      compiler:"Fedora gcc-aarch64-linux-gnu; bundled compilers excluded",
      status:"source prepared; binary build and hardware qualification not established"}' \
    >out/srpms/spi-vendor.json
[[ $mode != srpm ]] || exit 0
echo "Building unchanged vendor SPI with current Fedora tools; logs: $work/result/build.log"
./scripts/clean-rpm-work.bash "$work/result"
mkdir -p "$work/result" out/spi-vendor
build_status=0
./scripts/build-rpm.bash "$repo/packaging/mock/bpi-m4-f45-cross.cfg" \
    "$repo/out/srpms/spi-vendor.src.rpm" "$repo/$work" "bpi-m4-spi-vendor-$build_id" "$jobs" || build_status=$?
packages="$work/result/installed_pkgs.log"
[[ -f $packages ]] || packages=/dev/null
jq -n --slurpfile source out/srpms/spi-vendor.json --rawfile packages "$packages" \
    --argjson exit_status "$build_status" --arg log "$work/result/build.log" \
    '{source:$source[0],exit_status:$exit_status,
      result:(if $exit_status == 0 then "built for inspection" else "current-toolchain build failed" end),
      log:$log,build_tools:($packages|split("\n")|map(split(" ")[0] // "")|
        map(select(test("^(gcc-|gcc-c\\+\\+-|gcc-aarch64-linux-gnu-|openssl-|openssl-devel-)")))),
      hardware_flashed:false}' >out/spi-vendor/build-audit.json.tmp
mv -- out/spi-vendor/build-audit.json.tmp out/spi-vendor/build-audit.json
if (( build_status != 0 )); then
    echo 'SPI baseline build failed; audit: out/spi-vendor/build-audit.json' >&2
    exit "$build_status"
fi
echo "SPI updater built for local inspection: $work/result (not flashed)."
