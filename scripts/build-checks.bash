#!/usr/bin/bash
set -Eeuo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
cd "$repo"
export PATH="$repo/scripts/rpm-tools:$PATH"
work="$repo/work/rpms/checks"
./scripts/clean-rpm-work.bash "$work"
mkdir -p "$work/SOURCES" "$work/srpm-result" "$work/result" "$work/prepared/bpi-m4-spi-checks"
cp -a src patches scripts packaging tests licenses "$work/prepared/bpi-m4-spi-checks/"
tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -C "$work/prepared" -cf - bpi-m4-spi-checks | gzip -n >"$work/SOURCES/bpi-m4-spi-checks.tar.gz"
id=$(sha256sum "$work/SOURCES/bpi-m4-spi-checks.tar.gz")
./scripts/build-srpm.bash "$repo/packaging/rpkg.conf" "$repo/packaging/bpi-m4-build-checks.spec" "$work" "${id:0:12}" server 1
srpms=("$work/srpm-result/"*.src.rpm)
./scripts/build-rpm.bash "$repo/packaging/mock/bpi-m4-f45-cross.cfg" "${srpms[0]}" "$work" bpi-m4-spi-checks "${1:-8}"
